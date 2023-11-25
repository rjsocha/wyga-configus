# syntax=docker/dockerfile:1.6
FROM alpine AS build-layer
RUN apk --no-cache add gcc musl-dev make upx
COPY src/config-service.c src/httpserver.h /build/.wyga/src/
WORKDIR /build
RUN gcc -o config-service -s -O2 -static config-service.c && strip -x config-service && upx config-service
RUN cp config-service /dist/.entrypoint/config-service
COPY --chmod=0755 client/config-service /dist/client/config-service

FROM scratch
COPY --from=build-layer /dist/ /
USER 50000:50000
ENTRYPOINT [ "/.entrypoint/config-service" ]
