# build stage
FROM alpine:3.21.3 AS build

# args
ARG UNRAR6_VERSION=6.2.12
ARG UNRAR7_VERSION=7.1.6
ARG UNRAR7_NATIVE=false
ARG MAKE_JOBS=1
ARG TARGETPLATFORM
ARG CMAKE_BUILD_TYPE=Release

#install build packages
RUN \
  echo "**** install build packages ****" && \
  apk add g++ gcc git libxml2-dev libxslt-dev make ncurses-dev openssl-dev boost-dev curl cmake

#install archive packages
RUN \
  echo "**** install unrar7 from source ****" && \
  mkdir /tmp/unrar7 && \
  curl -o /tmp/unrar7.tar.gz -L "https://www.rarlab.com/rar/unrarsrc-${UNRAR7_VERSION}.tar.gz" && \
  tar xf /tmp/unrar7.tar.gz -C /tmp/unrar7 --strip-components=1 && \
  cd /tmp/unrar7 && \
  if [ "${UNRAR7_NATIVE}" != "true" ] && [ "${TARGETPLATFORM}" == "linux/amd64" ]; \
    then sed -i "s|CXXFLAGS=-march=native|CXXFLAGS=-march=x86-64-v2|" makefile; fi && \
  if [ "${UNRAR7_NATIVE}" != "true" ] && [ "${TARGETPLATFORM}" == "linux/arm64" ]; \
    then sed -i "s|CXXFLAGS=-march=native|CXXFLAGS=-march=armv8-a+crypto+crc|" makefile; fi && \
  if [ "${UNRAR7_NATIVE}" != "true" ] && [ "${TARGETPLATFORM}" == "linux/arm/v7" ]; \
    then sed -i "s|CXXFLAGS=-march=native|CXXFLAGS=-march=armv7-a|" makefile; fi && \
  make -j ${MAKE_JOBS} && \
  install -v -m755 unrar /usr/bin/unrar7 && \
  echo "**** install unrar6 from source ****" && \
  mkdir /tmp/unrar6 && \
  curl -o /tmp/unrar6.tar.gz -L "https://www.rarlab.com/rar/unrarsrc-${UNRAR6_VERSION}.tar.gz" && \
  tar xf /tmp/unrar6.tar.gz -C /tmp/unrar6 --strip-components=1 && \
  cd /tmp/unrar6 && \
  make -j ${MAKE_JOBS} && \
  install -v -m755 unrar /usr/bin/

#build nzbget
ADD ./ nzbget
RUN \
  echo "**** build nzbget ****" && \
  mkdir -p /app/nzbget && \
  mkdir -p /nzbget/build && \
  cd /nzbget/build && \
  cmake .. -DCMAKE_INSTALL_PREFIX=/app/nzbget -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} && \
  cmake --build . -j ${MAKE_JOBS} && \
  cmake --install . && \
  cmake --build . --target install-conf && \
  mv /app/nzbget/bin/nzbget /app/nzbget/ && \
  rm -rf /app/nzbget/bin/ && \
  rm -rf /app/nzbget/etc/ && \
  mv /app/nzbget/share/nzbget/webui /app/nzbget/ && \
  cp /app/nzbget/share/nzbget/nzbget.conf /app/nzbget/webui/nzbget.conf.template && \ 
  cp /nzbget/pubkey.pem /app/nzbget/pubkey.pem && \
  curl -o /app/nzbget/cacert.pem -L "https://curl.se/ca/cacert.pem"

#change default configvalues
RUN \
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
  /app/nzbget/share/nzbget/nzbget.conf

# runtime stage
FROM alpine:3.21.3
ARG NZBGET_RELEASE=develop

# labels
LABEL org.opencontainers.image.description="NZBGet from nzbget.com"
LABEL org.opencontainers.image.source="https://github.com/nzbgetcom/nzbget"
LABEL maintainer="nzbget@nzbget.com" 

ENV TERM=linux
COPY --from=build /usr/bin/unrar /usr/bin/unrar
COPY --from=build /usr/bin/unrar7 /usr/bin/unrar7
COPY --from=build /app/nzbget/ /app/nzbget/
RUN \
  echo "**** install packages ****" && \
  apk add --no-cache --update shadow libxml2 libxslt openssl 7zip python3 boost1.84-json tzdata && \
  ln -sf /usr/bin/python3 /usr/bin/python && \
  ln -s /usr/bin/7z /app/nzbget/7za && \
  ln -s /usr/bin/unrar /app/nzbget/unrar && \
  echo "**** cleanup ****" && \
  rm -rf /root/.cache /root/.cargo /tmp/*
ADD docker/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh && \
  echo "**** create non-root user ****" && \
  adduser -G users -D -u 1000 -h /config -s /bin/sh user && \
  mkdir -p /config && \
  mkdir -p /downloads && \
  chown -R user:users /app /config /downloads
EXPOSE 6789
ENTRYPOINT [ "/entrypoint.sh" ]
