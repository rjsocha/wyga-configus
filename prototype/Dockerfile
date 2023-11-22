# syntax=docker/dockerfile:1.5
FROM scratch AS build-layer
ARG TARGETOS
ARG TARGETARCH
COPY service.go /src/service.go
COPY Dockerfile /src/Dockerfile
COPY --chmod=0755 binary/config-service.${TARGETOS}.${TARGETARCH} /config-service
COPY --chmod=0755 client/config-service /client/config-service
FROM scratch
COPY --from=build-layer / /
ENTRYPOINT [ "/config-service" ]
