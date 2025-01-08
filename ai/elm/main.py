import numpy as np
import pandas as pd
from scipy.linalg import pinv
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import MinMaxScaler, OneHotEncoder, StandardScaler, RobustScaler, Normalizer

# Load the dataset
train = pd.read_csv('mnist_train.csv')
test = pd.read_csv('mnist_test.csv')
onehotencoder = OneHotEncoder(categories='auto')
scaler = MinMaxScaler()

# Split the dataset into training and validation sets
x_train = scaler.fit_transform(train.to_numpy()[:, 1:])  # Normalize pixel values
y_train = onehotencoder.fit_transform(train.to_numpy()[:, :1]).toarray()  # One-hot encode labels
x_test = scaler.transform(test.to_numpy()[:, 1:])  # Normalize pixel values
y_test = onehotencoder.transform(test.to_numpy()[:, :1]).toarray()  # One-hot encode labels

# Set up in the size of the input layer, number of hiddens neurons, and activation function
for hidden_size in [1000]:
  input_size = x_train.shape[1]
  input_weights = np.random.default_rng(12345).standard_normal((input_size, hidden_size))
  biases = np.random.default_rng(12345).standard_normal((hidden_size))
  def sigmoid(x):
      return 1 / (1 + np.exp(-x))
  def relu(x):
      return np.maximum(0, x)
  def sinerelu(x):
      return np.where(x > 0, x, 0.0025 * (np.sin(x) - np.cos(x)))
  def leaky_relu(x):
      return np.where(x > 0, x, x * 0.3)
  def gelu(x):
      return 0.5 * x * (1 + np.tanh(np.sqrt(2 / np.pi) * (x + 0.044715 * np.power(x, 3))))
  def elu(x, alpha=1.0):
      return np.where(x >= 0, x, alpha * (np.exp(x) - 1))
  def tanh(x):
      return np.tanh(x)
  def tanhre(x):
      return np.where(x > 0, x, np.tanh(x))
  def selu(x, elu_func=elu):
      return 1.0507009873554804934193349852946 * elu_func(x, 1.6732632423543772848170429916717)

  # Create the hidden layer vector
  def hidden_nodes(x):
      return relu(np.dot(x, input_weights) + biases)
  output_weights = np.dot(pinv(hidden_nodes(x_train)), y_train)

  # Create the prediction function
  def predict(x):
      hidden_layer = hidden_nodes(x)
      return np.dot(hidden_layer, output_weights)

  # Calculate the accuracy of the model
  correct = 0
  prediction = predict(x_test)
  total = x_test.shape[0]
  for i in range(total):
      if np.argmax(prediction[i]) == np.argmax(y_test[i]):
          correct += 1
  accuracy = correct / total
  print(f'Accuracy for {hidden_size} hidden nodes: {accuracy * 100:.2f}%')

