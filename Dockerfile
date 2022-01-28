FROM ubuntu:20.04


ARG DEBIAN_FRONTEND=noninteractive

# We need libpython2.7 due to GDB tools
# we also need npm 8 for the webapp to work
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
# pushd components/wifi-manager/webapp/ && npm install && npm run-script build && popd


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

# Now install nodejs, npm and the packages we need
COPY components/wifi-manager/webapp/package.json /opt

ENV NODE_VERSION 8

SHELL ["/bin/bash", "--login", "-c"]
# Install nvm with node and npm
# RUN wget -qO- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.1/install.sh | bash \
#     && export NVM_DIR="$([ -z "${XDG_CONFIG_HOME-}" ] && printf %s "${HOME}/.nvm" || printf %s "${XDG_CONFIG_HOME}/nvm")" \
#     && [ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh" \
#     && nvm install $NODE_VERSION \
#     && nvm alias default $NODE_VERSION \
#     && nvm use default \
#     && echo installing nodejs version 16  \
#     && curl -sL https://deb.nodesource.com/setup_16.x | bash - \
#     && echo installing node modules  \
#     && cd /opt \
#     && nvm use default \
#     && npm install -g \  
#     && :    

RUN : \
  && curl -fsSL https://deb.nodesource.com/setup_16.x | bash - \
  && apt-get install -y nodejs \
    && echo installing node modules  \
    && cd /opt \
    && npm i -g npm \
    && node --version \
    && npm install -g \  
    && :      

ENV NODE_PATH $NVM_DIR/v$NODE_VERSION/lib/node_modules
ENV PATH      $NVM_DIR/v$NODE_VERSION/bin:$PATH


ENTRYPOINT [ "/opt/esp/entrypoint.sh" ]
CMD [ "/bin/bash" ]
