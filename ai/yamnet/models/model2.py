# https://www.tensorflow.org/tutorials/audio/transfer_learning_audio


import mediapipe as mp
from mediapipe.tasks.python import audio

import pandas as pd
import tf_keras

import tensorflow as tf
import tensorflow_hub as hub
import tensorflow_io as tfio

print(f"TensorFlow Version: {tf.__version__}")
print(f"Model Maker Version: {mp.__version__}")

yamnet_model_handle = 'https://tfhub.dev/google/yamnet/1'
yamnet_model = hub.load(yamnet_model_handle)

@tf.function
def load_wav_16k_mono(filename):
    """ Load a WAV file, convert it to a float tensor, resample to 16 kHz single-channel audio. """
    file_contents = tf.io.read_file(filename)
    wav, sample_rate = tf.audio.decode_wav(file_contents, desired_channels=1)
    wav = tf.squeeze(wav, axis=-1)
    sample_rate = tf.cast(sample_rate, dtype=tf.int64)
    wav = tfio.audio.resample(wav, rate_in=sample_rate, rate_out=16000)
    return wav

testing_wav_file_name = tf.keras.utils.get_file('miaow_16k.wav',
                                                'https://storage.googleapis.com/audioset/miaow_16k.wav',
                                                cache_dir='./',
                                                cache_subdir='test_data')
testing_wav_data = load_wav_16k_mono(testing_wav_file_name)

yamnet_model = hub.load('https://tfhub.dev/google/yamnet/1')
class_map_path = yamnet_model.class_map_path().numpy().decode('utf-8')
class_names =list(pd.read_csv(class_map_path)['display_name'])

scores, embeddings, spectrogram = yamnet_model(testing_wav_data)
class_scores = tf.reduce_mean(scores, axis=0)
top_class = tf.math.argmax(class_scores)

print(f'The top score is: {class_scores[top_class]}')
print(f'The main sound is: {class_names[top_class]}')
print(f'The embeddings shape: {embeddings.shape}')






concrete_model = yamnet_model.__call__.get_concrete_function()
converter = tf.lite.TFLiteConverter.from_concrete_functions([concrete_model], concrete_model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
tflite_model = converter.convert()

model = tf.lite.Interpreter(model_content=tflite_model)
model.resize_tensor_input(model.get_input_details()[0]['index'], [len(testing_wav_data)])
model.allocate_tensors()
model.set_tensor(model.get_input_details()[0]['index'], testing_wav_data)
model.invoke()
scores = model.get_tensor(model.get_output_details()[0]['index'])
class_scores = tf.reduce_mean(scores, axis=0)
top_class = tf.math.argmax(class_scores)

print(f'The top score is: {class_scores[top_class]}')
print(f'The main sound is: {class_names[top_class]}')