# build stage
FROM alpine:3.18.4 AS build

# args
ARG NZBGET_RELEASE=develop
ARG UNRAR_VERSION=6.2.12
ARG MAKE_JOBS=1

RUN \
  echo "**** install build packages ****" && \
  apk add g++ gcc git libxml2-dev libxslt-dev make ncurses-dev openssl-dev curl && \
  echo "**** build nzbget ****" && \
  mkdir -p /app/nzbget && \
  git clone https://github.com/nzbgetcom/nzbget.git nzbget && \
  cd nzbget/ && \
  git checkout ${NZBGET_RELEASE} && \
  ./configure bindir='${exec_prefix}' && \
  make -j ${MAKE_JOBS} && \
  make prefix=/app/nzbget install && \
  sed -i \
    -e "s#^MainDir=.*#MainDir=/downloads#g" \
    -e "s#^ScriptDir=.*#ScriptDir=$\{MainDir\}/scripts#g" \
    -e "s#^WebDir=.*#WebDir=$\{AppDir\}/webui#g" \
    -e "s#^ConfigTemplate=.*#ConfigTemplate=$\{AppDir\}/webui/nzbget.conf.template#g" \
    -e "s#^UnrarCmd=.*#UnrarCmd=$\{AppDir\}/unrar#g" \
    -e "s#^SevenZipCmd=.*#SevenZipCmd=$\{AppDir\}/7za#g" \
    -e "s#^CertStore=.*#CertStore=$\{AppDir\}/cacert.pem#g" \
    -e "s#^CertCheck=.*#CertCheck=yes#g" \
    -e "s#^DestDir=.*#DestDir=$\{MainDir\}/completed#g" \
    -e "s#^InterDir=.*#InterDir=$\{MainDir\}/intermediate#g" \
    -e "s#^LogFile=.*#LogFile=$\{MainDir\}/nzbget.log#g" \
    -e "s#^AuthorizedIP=.*#AuthorizedIP=127.0.0.1#g" \
  /app/nzbget/share/nzbget/nzbget.conf && \
  mv /app/nzbget/share/nzbget/webui /app/nzbget/ && \
  cp /app/nzbget/share/nzbget/nzbget.conf /app/nzbget/webui/nzbget.conf.template && \
  ln -s /usr/bin/7za /app/nzbget/7za && \
  ln -s /usr/bin/unrar /app/nzbget/unrar && \
  cp /nzbget/pubkey.pem /app/nzbget/pubkey.pem && \
  curl -o /app/nzbget/cacert.pem -L "https://curl.se/ca/cacert.pem" && \
  echo "**** install unrar from source ****" && \
  mkdir /tmp/unrar && \
  curl -o /tmp/unrar.tar.gz -L "https://www.rarlab.com/rar/unrarsrc-${UNRAR_VERSION}.tar.gz" && \  
  tar xf /tmp/unrar.tar.gz -C /tmp/unrar --strip-components=1 && \
  cd /tmp/unrar && \
  make -j ${MAKE_JOBS} && \
  install -v -m755 unrar /usr/bin

# runtime stage
FROM alpine:3.18.4
ARG NZBGET_RELEASE=develop

# labels
LABEL org.opencontainers.image.description="NZBGet from nzbget.com, version ${NZBGET_RELEASE}"
LABEL org.opencontainers.image.source="https://github.com/nzbgetcom/nzbget"
LABEL maintainer="nzbget@nzbget.com" 

ENV TERM linux
RUN \
  echo "**** install packages ****" && \
  apk add --no-cache --update shadow libxml2 libxslt openssl p7zip python3 && \
  ln -sf /usr/bin/python3 /usr/bin/python && \
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
