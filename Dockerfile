FROM ubuntu:20.04


ARG DEBIAN_FRONTEND=noninteractive

COPY components/wifi-manager/webapp/package.json /opt

# We need libpython2.7 due to GDB tools
RUN : \
  && apt-get update \
  && apt-get install -y \
    apt-utils \
    bison \
    ca-certificates \
    ccache \
    check \
    curl \
    flex \
    git \
    gperf \
    lcov \
    libffi-dev \
    libncurses-dev \
    libpython2.7 \
    libusb-1.0-0-dev \
    make \
    ninja-build \
    python3 \
    python3-pip \
    unzip \
    wget \
    xz-utils \
    zip \
	npm \
	nodejs \
  && apt-get autoremove -y \
  && rm -rf /var/lib/apt/lists/* \
  && update-alternatives --install /usr/bin/python python /usr/bin/python3 10 \
  && python -m pip install --upgrade \
    pip \
    virtualenv \
  && cd /opt \
  && npm install -g \
  && :

# To build the image for a branch or a tag of IDF, pass --build-arg IDF_CLONE_BRANCH_OR_TAG=name.
# To build the image with a specific commit ID of IDF, pass --build-arg IDF_CHECKOUT_REF=commit-id.
# It is possibe to combine both, e.g.:
#   IDF_CLONE_BRANCH_OR_TAG=release/vX.Y
#   IDF_CHECKOUT_REF=<some commit on release/vX.Y branch>.
# The following commit contains the ldgen fix: eab738c79e063b3d6f4c345ea5e1d4f8caef725b
# to build an image using that commit: docker build . --build-arg IDF_CHECKOUT_REF=eab738c79e063b3d6f4c345ea5e1d4f8caef725b -t sle118/squeezelite-esp32-idfv4-master
# docker build . --build-arg IDF_CHECKOUT_REF=8bf14a9238329954c7c5062eeeda569529aedf75  -t sle118/squeezelite-esp32-idfv4-master
# To run the image interactive (windows): docker run --rm -v %cd%:/project -w /project -it sle118/squeezelite-esp32-idfv4-master
# to build the web app inside of the interactive session
# pushd components/wifi-manager/webapp/ && npm rebuild node-sass && npm run-script build && popd

ARG IDF_CLONE_URL=https://github.com/espressif/esp-idf.git
ARG IDF_CLONE_BRANCH_OR_TAG=master
ARG IDF_CHECKOUT_REF=eab738c79e063b3d6f4c345ea5e1d4f8caef725b

ENV IDF_PATH=/opt/esp/idf
ENV IDF_TOOLS_PATH=/opt/esp

RUN echo IDF_CHECKOUT_REF=$IDF_CHECKOUT_REF IDF_CLONE_BRANCH_OR_TAG=$IDF_CLONE_BRANCH_OR_TAG && \
    git clone --recursive \
      ${IDF_CLONE_BRANCH_OR_TAG:+-b $IDF_CLONE_BRANCH_OR_TAG} \
      $IDF_CLONE_URL $IDF_PATH && \
	  if [ -n "$IDF_CHECKOUT_REF" ]; then \
      cd $IDF_PATH && \
      git checkout $IDF_CHECKOUT_REF && \
      git submodule update --init --recursive; \
    fi 
COPY docker/patches $IDF_PATH



# Install all the required tools
RUN : \
  && update-ca-certificates --fresh \
  && $IDF_PATH/tools/idf_tools.py --non-interactive install required \
  && $IDF_PATH/tools/idf_tools.py --non-interactive install cmake \
  && $IDF_PATH/tools/idf_tools.py --non-interactive install-python-env \
  && rm -rf $IDF_TOOLS_PATH/dist \
  && :

# Ccache is installed, enable it by default
ENV IDF_CCACHE_ENABLE=1
COPY docker/entrypoint.sh /opt/esp/entrypoint.sh

ENTRYPOINT [ "/opt/esp/entrypoint.sh" ]
CMD [ "/bin/bash" ]
