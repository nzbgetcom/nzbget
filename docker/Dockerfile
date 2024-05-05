# build stage
FROM alpine:3.19.1 AS build

# args
ARG NZBGET_RELEASE=develop
ARG UNRAR_VERSION=7.0.7
ARG UNRAR_NATIVE=false
ARG MAKE_JOBS=32
ARG TARGETPLATFORM

RUN \
  echo "**** install build packages ****" && \
  apk add g++ gcc git libxml2-dev libxslt-dev make ncurses-dev openssl-dev boost-dev curl cmake && \
  echo "**** build nzbget ****" && \
  mkdir -p /app/nzbget && \
  git clone https://github.com/nzbgetcom/nzbget.git nzbget && \
  cd nzbget/ && \
  git checkout ${NZBGET_RELEASE} && \
  mkdir -p build && \
  cd build && \
  cmake .. -DCMAKE_INSTALL_PREFIX=/app/nzbget && \
  cmake --build . -j ${MAKE_JOBS} && \
  cmake --install . && \
  mv /app/nzbget/bin/nzbget /app/nzbget/ && \
  rm -rf /app/nzbget/bin/ && \
  rm -rf /app/nzbget/etc/ && \
  sed -i \
    -e "s|^MainDir=.*|MainDir=/downloads|g" \
    -e "s|^ScriptDir=.*|ScriptDir=$\{MainDir\}/scripts|g" \
    -e "s|^WebDir=.*|WebDir=$\{AppDir\}/webui|g" \
    -e "s|^ConfigTemplate=.*|ConfigTemplate=$\{AppDir\}/webui/nzbget.conf.template|g" \
    -e "s|^UnrarCmd=.*|UnrarCmd=unrar|g" \
    -e "s|^SevenZipCmd=.*|SevenZipCmd=7z|g" \
    -e "s|^CertStore=.*|CertStore=$\{AppDir\}/cacert.pem|g" \
    -e "s|^CertCheck=.*|CertCheck=yes|g" \
    -e "s|^DestDir=.*|DestDir=$\{MainDir\}/completed|g" \
    -e "s|^InterDir=.*|InterDir=$\{MainDir\}/intermediate|g" \
    -e "s|^LogFile=.*|LogFile=$\{MainDir\}/nzbget.log|g" \
    -e "s|^AuthorizedIP=.*|AuthorizedIP=127.0.0.1|g" \
  /app/nzbget/share/nzbget/nzbget.conf && \
  mv /app/nzbget/share/nzbget/webui /app/nzbget/ && \
  cp /app/nzbget/share/nzbget/nzbget.conf /app/nzbget/webui/nzbget.conf.template && \
  ln -s /usr/bin/7z /app/nzbget/7za && \
  ln -s /usr/bin/unrar /app/nzbget/unrar && \
  cp /nzbget/pubkey.pem /app/nzbget/pubkey.pem && \
  curl -o /app/nzbget/cacert.pem -L "https://curl.se/ca/cacert.pem" && \
  echo "**** install unrar from source ****" && \
  mkdir /tmp/unrar && \
  curl -o /tmp/unrar.tar.gz -L "https://www.rarlab.com/rar/unrarsrc-${UNRAR_VERSION}.tar.gz" && \  
  tar xf /tmp/unrar.tar.gz -C /tmp/unrar --strip-components=1 && \
  cd /tmp/unrar && \
  if [ "${UNRAR_NATIVE}" != "true" ] && [ "${TARGETPLATFORM}" == "linux/amd64" ]; \
    then sed -i "s|CXXFLAGS=-march=native|CXXFLAGS=-march=x86-64-v2|" makefile; fi && \
  if [ "${UNRAR_NATIVE}" != "true" ] && [ "${TARGETPLATFORM}" == "linux/arm64" ]; \
    then sed -i "s|CXXFLAGS=-march=native|CXXFLAGS=-march=armv8-a|" makefile; fi && \
  if [ "${UNRAR_NATIVE}" != "true" ] && [ "${TARGETPLATFORM}" == "linux/arm/v7" ]; \
    then sed -i "s|CXXFLAGS=-march=native|CXXFLAGS=-march=armv7-a|" makefile; fi && \
  make -j ${MAKE_JOBS} && \
  install -v -m755 unrar /usr/bin

# runtime stage
FROM alpine:3.19.1
ARG NZBGET_RELEASE=develop

# labels
LABEL org.opencontainers.image.description="NZBGet from nzbget.com, version ${NZBGET_RELEASE}"
LABEL org.opencontainers.image.source="https://github.com/nzbgetcom/nzbget"
LABEL maintainer="nzbget@nzbget.com" 

ENV TERM linux
COPY beets-1.6.0.tar.gz /tmp 
RUN \
  echo "**** install packages ****" && \
  apk add --no-cache --update shadow libxml2 libxslt openssl 7zip python3 boost1.82-json tzdata build-base gcc linux-headers musl-dev openjdk17-jre-headless openssl py3-pip python3 python3-dev py3-wheel && \
  ln -sf /usr/bin/python3 /usr/bin/python && \
  echo "**** install python packages ****" && \
  cd /tmp && \
  tar xzvf beets-1.6.0.tar.gz && \
  cd beets-1.6.0 && \
  touch `find . -type f` && \
  python setup.py install && \
  echo "**** cleanup ****" && \
  rm -rf /root/.cache /root/.cargo /tmp/*
COPY --from=build /usr/bin/unrar /usr/bin/unrar
COPY --from=build /app/nzbget/ /app/nzbget/
ADD entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh && \
  echo "**** create non-root user ****" && \
  adduser -G users -D -u 1000 -h /config -s /bin/sh user && \
  mkdir -p /config && \
  mkdir -p /downloads && \
  chown -R user:users /app /config /downloads
ENTRYPOINT [ "/entrypoint.sh" ]
