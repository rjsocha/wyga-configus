# syntax=docker/dockerfile:1.6
FROM alpine AS build-layer
RUN apk --no-cache add gcc musl-dev make upx
WORKDIR /build
COPY src/configus.c src/httpserver.h .
RUN gcc -o configus -s -O2 -static configus.c && strip -x configus
WORKDIR /dist/.entrypoint
RUN cp /build/configus configus
COPY --chmod=0755 client/config-service /dist/client/config-service

FROM scratch
COPY --from=build-layer /dist/ /
USER 50000:50000
ENTRYPOINT [ "/.entrypoint/configus" ]
