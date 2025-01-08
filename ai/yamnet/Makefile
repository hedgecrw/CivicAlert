export DOCKER_DEFAULT_PLATFORM=linux/amd64
docker run --name tensorflow --volume='/Volumes/External HD/Dropbox':/opt/Dropbox -it ubuntu:latest bash
docker start -i tensorflow



# Ubuntu
python3 -m pip install tensorflow tensorflow-io tensorflow-hub seaborn mediapipe

# Mac (x86_64)
python3 -m pip install tensorflow==2.16.2 tensorflow-metal tensorflow-io tensorflow-hub seaborn mediapipe

# Mac (arm64)
python3.10 -m venv ~/tf
source ~/tf/bin/activate
export ARCHFLAGS="-arch arm64"
export TF_PYTHON_VERSION=3.10
pip install tensorflow==2.16.2 tensorflow-metal tensorflow-hub seaborn mediapipe wheel setuptools
wget https://github.com/tensorflow/io/archive/refs/tags/v0.37.1.zip
unzip v0.37.1.zip
cd io-0.37.1
In tools/build/configure.py and tools/build/swift/BUILD: change all x86_64-apple-macosx to arm64-apple-macosx14.5
In tensorflow_io/BUILD: Delete line: "@bazel_tools//src/conditions:darwin_arm64": [],
In third_party/libmongoc.BUILD, add on line after '#include "openssl/bio.h"', '#include "openssl/ssl.h"',
./configure.sh
bazel build -s --verbose_failures --config=cache //tensorflow_io/... //tensorflow_io_gcs_filesystem/...
python3 setup.py --data bazel-bin -q bdist_wheel --plat-name macosx_14_0_arm64
rm -rf build
python3 setup.py --project tensorflow-io-gcs-filesystem --data bazel-bin -q bdist_wheel --plat-name macosx_14_0_arm64
Wheels in dist/ (python3 -m pip install dist/.....whl)