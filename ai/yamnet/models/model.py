import matplotlib.pyplot as plt
import pandas as pd
from sklearn.metrics import confusion_matrix, classification_report


import resampy
import numpy as np
import soundfile as sf
import tensorflow as tf
from tf_keras import Model, layers


sample_rate: float = 16000.0
stft_window_seconds: float = 0.025
stft_hop_seconds: float = 0.010
mel_bands: int = 64
mel_min_hz: float = 125.0
mel_max_hz: float = 7500.0
log_offset: float = 0.001
patch_window_seconds: float = 0.96
patch_hop_seconds: float = 0.48
num_classes: int = 521
conv_padding: str = 'same'
batchnorm_center: bool = True
batchnorm_scale: bool = False
batchnorm_epsilon: float = 1e-4
classifier_activation: str = 'sigmoid'

def patch_frames():
  return int(round(patch_window_seconds / stft_hop_seconds))


def waveform_to_features(waveform):
  with tf.name_scope('log_mel_features'):
    # Convert waveform into spectrogram using a Short-Time Fourier Transform
    window_length_samples = int(round(sample_rate * stft_window_seconds))
    hop_length_samples = int(round(sample_rate * stft_hop_seconds))
    fft_length = 2 ** int(np.ceil(np.log(window_length_samples) / np.log(2.0)))
    magnitude_spectrogram = _tflite_stft_magnitude(signal=waveform,
                                                   frame_length=window_length_samples,
                                                   frame_step=hop_length_samples,
                                                   fft_length=fft_length)

    # Convert spectrogram into log mel spectrogram
    linear_to_mel_weight_matrix = \
      tf.signal.linear_to_mel_weight_matrix(num_mel_bins=mel_bands,
                                            num_spectrogram_bins=fft_length // 2 + 1,
                                            sample_rate=sample_rate,
                                            lower_edge_hertz=mel_min_hz,
                                            upper_edge_hertz=mel_max_hz)
    mel_spectrogram = tf.matmul(magnitude_spectrogram, linear_to_mel_weight_matrix)
    log_mel_spectrogram = tf.math.log(mel_spectrogram + log_offset)

    # Combine spectrograms into input patches
    spectrogram_hop_length_samples = int(round(sample_rate * stft_hop_seconds))
    spectrogram_sample_rate = sample_rate / spectrogram_hop_length_samples
    patch_window_length_samples = int(round(spectrogram_sample_rate * patch_window_seconds))
    patch_hop_length_samples = int(round(spectrogram_sample_rate * patch_hop_seconds))
    return tf.signal.frame(signal=log_mel_spectrogram,
                           frame_length=patch_window_length_samples,
                           frame_step=patch_hop_length_samples,
                           axis=0)


def pad_waveform(waveform):
  # In order to produce one patch of log mel spectrogram input to YAMNet, we
  # need at least one patch window length of waveform plus enough extra samples
  # to complete the final STFT analysis window
  min_waveform_seconds = (patch_window_seconds + stft_window_seconds - stft_hop_seconds)
  min_num_samples = tf.cast(min_waveform_seconds * sample_rate, tf.int32)
  num_samples = tf.shape(waveform)[0]
  num_padding_samples = tf.maximum(0, min_num_samples - num_samples)

  # In addition, there might be enough waveform for one or more additional
  # patches formed by hopping forward. If there are more samples than one patch,
  # round up to an integral number of hops.
  num_samples = tf.maximum(num_samples, min_num_samples)
  num_samples_after_first_patch = num_samples - min_num_samples
  hop_samples = tf.cast(patch_hop_seconds * sample_rate, tf.int32)
  num_hops_after_first_patch = tf.cast(tf.math.ceil(
    tf.cast(num_samples_after_first_patch, tf.float32) / tf.cast(hop_samples, tf.float32)), tf.int32)
  num_padding_samples += (hop_samples * num_hops_after_first_patch - num_samples_after_first_patch)
  return tf.pad(waveform, [[0, num_padding_samples]], mode='CONSTANT', constant_values=0.0)


def _tflite_stft_magnitude(signal, frame_length, frame_step, fft_length):
  """TF-Lite-compatible version of tf.abs(tf.signal.stft())."""
  def _hann_window():
    return tf.reshape(tf.constant((0.5 - 0.5 * np.cos(2 * np.pi * np.arange(0, 1.0, 1.0 / frame_length))).astype(np.float32),
                                  name='hann_window'), [1, frame_length])

  def _dft_matrix(dft_length):
    """Calculate the full DFT matrix in NumPy."""
    # See https://en.wikipedia.org/wiki/DFT_matrix
    omega = (0 + 1j) * 2.0 * np.pi / float(dft_length)
    # Don't include 1/sqrt(N) scaling, tf.signal.rfft doesn't apply it.
    return np.exp(omega * np.outer(np.arange(dft_length), np.arange(dft_length)))

  def _rdft(framed_signal, fft_length):
    """Implement real-input Discrete Fourier Transform by matmul."""
    # We are right-multiplying by the DFT matrix, and we are keeping only the
    # first half ("positive frequencies"). So discard the second half of rows,
    # but transpose the array for right-multiplication.  The DFT matrix is
    # symmetric, so we could have done it more directly, but this reflects our
    # intention better.
    complex_dft_matrix_kept_values = _dft_matrix(fft_length)[:(fft_length // 2 + 1), :].transpose()
    real_dft_matrix = tf.constant(np.real(complex_dft_matrix_kept_values).astype(np.float32),
                                  name='real_dft_matrix')
    imag_dft_matrix = tf.constant(np.imag(complex_dft_matrix_kept_values).astype(np.float32),
                                  name='imaginary_dft_matrix')
    signal_frame_length = tf.shape(framed_signal)[-1]
    half_pad = (fft_length - signal_frame_length) // 2
    padded_frames = tf.pad(framed_signal,
                           [# Don't add any padding in the frame dimension.
                            [0, 0],
                            # Pad before and after the signal within each frame.
                            [half_pad, fft_length - signal_frame_length - half_pad]
                           ],
                           mode='CONSTANT', constant_values=0.0)
    real_stft = tf.matmul(padded_frames, real_dft_matrix)
    imag_stft = tf.matmul(padded_frames, imag_dft_matrix)
    return real_stft, imag_stft

  def _complex_abs(real, imag):
    return tf.sqrt(tf.add(real * real, imag * imag))

  framed_signal = tf.signal.frame(signal, frame_length, frame_step)
  windowed_signal = framed_signal * _hann_window()
  real_stft, imag_stft = _rdft(windowed_signal, fft_length)
  stft_magnitude = _complex_abs(real_stft, imag_stft)
  return stft_magnitude


def _batch_norm(name, layer_input):
  return layers.BatchNormalization(
    name=name,
    center=batchnorm_center,
    scale=batchnorm_scale,
    epsilon=batchnorm_epsilon)(layer_input)


def _conv(name, kernel, stride, filters, layer_input):
  output = layers.Conv2D(name='{}/conv'.format(name),
                         filters=filters,
                         kernel_size=kernel,
                         strides=stride,
                         padding=conv_padding,
                         use_bias=False,
                         activation=None)(layer_input)
  output = _batch_norm('{}/conv/bn'.format(name), output)
  output = layers.ReLU(name='{}/relu'.format(name))(output)
  return output


def _separable_conv(name, kernel, stride, filters, layer_input):
  output = layers.DepthwiseConv2D(name='{}/depthwise_conv'.format(name),
                                  kernel_size=kernel,
                                  strides=stride,
                                  depth_multiplier=1,
                                  padding=conv_padding,
                                  use_bias=False,
                                  activation=None)(layer_input)
  output = _batch_norm('{}/depthwise_conv/bn'.format(name), output)
  output = layers.ReLU(name='{}/depthwise_conv/relu'.format(name))(output)
  output = layers.Conv2D(name='{}/pointwise_conv'.format(name),
                         filters=filters,
                         kernel_size=(1, 1),
                         strides=1,
                         padding=conv_padding,
                         use_bias=False,
                         activation=None)(output)
  output = _batch_norm('{}/pointwise_conv/bn'.format(name), output)
  output = layers.ReLU(name='{}/pointwise_conv/relu'.format(name))(output)
  return output


_YAMNET_LAYER_DEFS = [
    # (layer_function, kernel, stride, num_filters)
    (_conv,           [3, 3], 2,   32),
    (_separable_conv, [3, 3], 1,   64),
    (_separable_conv, [3, 3], 2,  128),
    (_separable_conv, [3, 3], 1,  128),
    (_separable_conv, [3, 3], 2,  256),
    (_separable_conv, [3, 3], 1,  256),
    (_separable_conv, [3, 3], 2,  512),
    (_separable_conv, [3, 3], 1,  512),
    (_separable_conv, [3, 3], 1,  512),
    (_separable_conv, [3, 3], 1,  512),
    (_separable_conv, [3, 3], 1,  512),
    (_separable_conv, [3, 3], 1,  512),
    (_separable_conv, [3, 3], 2, 1024),
    (_separable_conv, [3, 3], 1, 1024)
]


def yamnet(features):
  net = layers.Reshape((patch_frames(), mel_bands, 1),
                       input_shape=(patch_frames(), mel_bands))(features)
  for (i, (layer_fun, kernel, stride, filters)) in enumerate(_YAMNET_LAYER_DEFS):
    net = layer_fun('layer{}'.format(i + 1), kernel, stride, filters, net)
  embeddings = layers.GlobalAveragePooling2D()(net)
  logits = layers.Dense(units=num_classes, use_bias=True)(embeddings)
  predictions = layers.Activation(activation=classifier_activation)(logits)
  return predictions, embeddings


def yamnet_frames_model():
  waveform = layers.Input(batch_shape=(None,), dtype=tf.float32)
  waveform_padded = pad_waveform(waveform)
  features = waveform_to_features(waveform_padded)
  predictions, embeddings = yamnet(features)
  frames_model = Model(name='yamnet_frames', inputs=waveform, outputs=[predictions, embeddings])
  return frames_model




model = yamnet_frames_model()

def preprocess_wav(filename):
  wav_data, sr = sf.read(filename, dtype=np.int16)
  assert wav_data.dtype == np.int16, 'Bad sample type: %r' % wav_data.dtype
  waveform = wav_data / 32768.0  # Convert to [-1.0, +1.0]
  waveform = waveform.astype('float32')
  if len(waveform.shape) > 1:
    waveform = np.mean(waveform, axis=1)
  if sr != sample_rate:
    waveform = resampy.resample(waveform, sr, sample_rate)
  return waveform
  #return tf.convert_to_tensor(waveform, dtype=tf.float32)




#
esc50 = pd.read_csv('esc50/meta/esc50.csv')
animals = ['dog', 'rooster', 'pig', 'cow', 'frog', 'cat', 'hen', 'insects', 'sheep', 'crow']
map_class_to_id = {'dog':0,'rooster':1, 'pig':2, 'cow':3, 'frog':4, 'cat':5, 'hen':6, 'insects':7, 'sheep':8, 'crow':9}
animals = esc50[esc50.category.isin(animals)]
class_id = animals['category'].apply(lambda name: map_class_to_id[name])
animals = animals.assign(target=class_id)

esc50 = tf.data.Dataset.from_tensor_slices((animals['filename'], animals['target'], animals['fold']))
def load_wav_for_map(filename, label, fold):
  return tf.py_function(preprocess_wav, [filename], tf.float32), label, fold
esc50 = esc50.map(load_wav_for_map)
print(esc50.element_spec)

yamnet_model_handle = 'https://tfhub.dev/google/yamnet/1'
yamnet = hub.load(yamnet_model_handle)