FROM ubuntu:20.04


ARG DEBIAN_FRONTEND=noninteractive
ENV GCC_TOOLS_BASE=/opt/esp/tools/xtensa-esp32-elf/esp-2021r2-patch3-8.4.0/xtensa-esp32-elf/bin/xtensa-esp32-elf-
# To build the image for a branch or a tag of IDF, pass --build-arg IDF_CLONE_BRANCH_OR_TAG=name.
# To build the image with a specific commit ID of IDF, pass --build-arg IDF_CHECKOUT_REF=commit-id.
# It is possibe to combine both, e.g.:
#   IDF_CLONE_BRANCH_OR_TAG=release/vX.Y
#   IDF_CHECKOUT_REF=<some commit on release/vX.Y branch>.
# Docker build for release 4.3.5 as of 2023/05/18
# docker build . --build-arg IDF_CHECKOUT_REF=6d04316cbe4dc35ea7e4885e9821bd9958ac996d -t sle118/squeezelite-esp32-idfv435 
# Updating the docker image in the repository
# docker push sle118/squeezelite-esp32-idfv435
# or to do both:
# docker build . --build-arg IDF_CHECKOUT_REF=6d04316cbe4dc35ea7e4885e9821bd9958ac996d -t sle118/squeezelite-esp32-idfv435 && docker push sle118/squeezelite-esp32-idfv435
#
# (windows) To run the image interactive : 
# docker run --rm -v %cd%:/project -w /project -it sle118/squeezelite-esp32-idfv435
# (windows powershell)
# docker run --rm -v ${PWD}:/project -w /project -it sle118/squeezelite-esp32-idfv435
# (linux) To run the image interactive :
# docker run --rm -v `pwd`:/project -w /project -it sle118/squeezelite-esp32-idfv435
# to build the web app inside of the interactive session
# pushd components/wifi-manager/webapp/ && npm install && npm run-script build && popd
#
# to run the docker with netwotrk port published on the host:
# (windows)
# docker run --rm -p 5000:5000/tcp -v %cd%:/project -w /project -it sle118/squeezelite-esp32-idfv435
# (linux)
# docker run --rm -p 5000:5000/tcp -v `pwd`:/project -w /project -it sle118/squeezelite-esp32-idfv435


ARG IDF_CLONE_URL=https://github.com/espressif/esp-idf.git
ARG IDF_CLONE_BRANCH_OR_TAG=master
ARG IDF_CHECKOUT_REF=6d04316cbe4dc35ea7e4885e9821bd9958ac996d

ENV IDF_PATH=/opt/esp/idf
ENV IDF_TOOLS_PATH=/opt/esp

# We need libpython2.7 due to GDB tools
# we also need npm 8 for the webapp to work
RUN : \
  && apt-get update \
  && apt-get install -y \
    apt-utils \
    build-essential \
    bison \
    ca-certificates \
    ccache \
    check \
    curl \
    flex \
    git \
    git-lfs \    
    gperf \
    lcov \
    libbsd-dev \    
    libpython3.8 \
    libffi-dev \
    libncurses-dev \
    libusb-1.0-0-dev \
    make \
    ninja-build \
    python3.8 \
    python3-pip \
    python3-venv \
    ruby \
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
  && git clone https://github.com/HBehrens/puncover.git \
  && cd puncover \
  && python setup.py -q install \
  && echo IDF_CHECKOUT_REF=$IDF_CHECKOUT_REF IDF_CLONE_BRANCH_OR_TAG=$IDF_CLONE_BRANCH_OR_TAG \
  && git clone --recursive \
      ${IDF_CLONE_BRANCH_OR_TAG:+-b $IDF_CLONE_BRANCH_OR_TAG} \
      $IDF_CLONE_URL $IDF_PATH \
	&& if [ -n "$IDF_CHECKOUT_REF" ]; then \
      cd $IDF_PATH \
  &&  git checkout $IDF_CHECKOUT_REF \
  &&  git submodule update --init --recursive; \
    fi \
  && update-ca-certificates --fresh \
  && $IDF_PATH/tools/idf_tools.py --non-interactive install required \
  && $IDF_PATH/tools/idf_tools.py --non-interactive install cmake \
  && $IDF_PATH/tools/idf_tools.py --non-interactive install-python-env \
  && :
RUN : \
  echo Installing pygit2  ******************************************************** \
  && . /opt/esp/python_env/idf4.3_py3.8_env/bin/activate \
  && ln -sf /opt/esp/python_env/idf4.3_py3.8_env/bin/python  /usr/local/bin/python \
  && pip install pygit2 requests \
  && pip show pygit2 \ 
  && python --version \  
  && pip --version \
  && pip install protobuf  grpcio-tools \
  && rm -rf $IDF_TOOLS_PATH/dist \
  && :

COPY docker/patches $IDF_PATH

#set idf environment variabies
ENV PATH /opt/esp/idf/components/esptool_py/esptool:/opt/esp/idf/components/espcoredump:/opt/esp/idf/components/partition_table:/opt/esp/idf/components/app_update:/opt/esp/tools/xtensa-esp32-elf/esp-2021r2-patch3-8.4.0/xtensa-esp32-elf/bin:/opt/esp/tools/xtensa-esp32s2-elf/esp-2021r2-patch3-8.4.0/xtensa-esp32s2-elf/bin:/opt/esp/tools/xtensa-esp32s3-elf/esp-2021r2-patch3-8.4.0/xtensa-esp32s3-elf/bin:/opt/esp/tools/riscv32-esp-elf/esp-2021r2-patch3-8.4.0/riscv32-esp-elf/bin:/opt/esp/tools/esp32ulp-elf/2.28.51-esp-20191205/esp32ulp-elf-binutils/bin:/opt/esp/tools/esp32s2ulp-elf/2.28.51-esp-20191205/esp32s2ulp-elf-binutils/bin:/opt/esp/tools/cmake/3.16.4/bin:/opt/esp/tools/openocd-esp32/v0.11.0-esp32-20220706/openocd-esp32/bin:/opt/esp/python_env/idf4.3_py3.8_env/bin:/opt/esp/idf/tools:$PATH
ENV GCC_TOOLS_BASE="/opt/esp/tools/xtensa-esp32-elf/esp-2021r2-patch3-8.4.0/xtensa-esp32-elf/bin/xtensa-esp32-elf-"
ENV IDF_PATH="/opt/esp/idf"
ENV IDF_PYTHON_ENV_PATH="/opt/esp/python_env/idf4.3_py3.8_env"
ENV IDF_TOOLS_EXPORT_CMD="/opt/esp/idf/export.sh"
ENV IDF_TOOLS_INSTALL_CMD="/opt/esp/idf/install.sh"
ENV IDF_TOOLS_PATH="/opt/esp"
ENV NODE_PATH="/v8/lib/node_modules"
ENV NODE_VERSION="8"
ENV OPENOCD_SCRIPTS="/opt/esp/tools/openocd-esp32/v0.10.0-esp32-20211111/openocd-esp32/share/openocd/scripts"
# Ccache is installed, enable it by default

# The constraint file has been downloaded and the right Python package versions installed. No need to check and
# download this at every invocation of the container.
ENV IDF_PYTHON_CHECK_CONSTRAINTS=no

# Ccache is installed, enable it by default
ENV IDF_CCACHE_ENABLE=1

# Install QEMU runtime dependencies
RUN : \
  && apt-get update && apt-get install -y -q \
    libglib2.0-0 \
    libpixman-1-0 \
  && rm -rf /var/lib/apt/lists/* \
  && :

# Install QEMU
ARG QEMU_VER=esp-develop-20220919
ARG QEMU_DIST=qemu-${QEMU_VER}.tar.bz2
ARG QEMU_SHA256=f6565d3f0d1e463a63a7f81aec94cce62df662bd42fc7606de4b4418ed55f870
RUN : \
  && wget --no-verbose https://github.com/espressif/qemu/releases/download/${QEMU_VER}/${QEMU_DIST} \
  && echo "${QEMU_SHA256} *${QEMU_DIST}" | sha256sum --check --strict - \
  && tar -xf ${QEMU_DIST} -C /opt \
  && rm ${QEMU_DIST} \
  && :

COPY docker/entrypoint.sh /opt/esp/entrypoint.sh
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
  && apt-get install -y nodejs jq \
  && echo installing dev node modules globally \
  && cd /opt \
  && cat ./package.json | jq '.devDependencies | keys[] as $k | "\($k)@\(.[$k])"' | xargs -t npm install --global \
  && echo installing npm global packages \
  && npm i -g npm \
  && node --version \
  && npm install -g \  
  && :      
RUN : \
  && npm install -g html-webpack-plugin 


ENV NODE_PATH $NVM_DIR/v$NODE_VERSION/lib/node_modules
ENV PATH $IDF_PYTHON_ENV_PATH:$NVM_DIR/v$NODE_VERSION/bin:$PATH
COPY ./docker/build_tools.py /usr/sbin/build_tools.py
RUN : \
  && echo Changing permissions ********************************************************  \
  && chmod +x /opt/esp/entrypoint.sh \
  && chmod +x /usr/sbin/build_tools.py \  
  && :



ENTRYPOINT [ "/opt/esp/entrypoint.sh" ]
CMD [ "/bin/bash" ]
