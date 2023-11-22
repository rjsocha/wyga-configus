# configus

## Image
```
wyga/configus
```

## Usage

```
FROM wyga/configus
COPY config /config/service/config
COPY sample.conf /config/service/sample.conf
```
config file:
```
:GET src dst
```

For example:
```
:GET nginx/default.conf /config/nginx/default.conf
```

Env variables are evaluated for src and dst when they are in format:
```
${VARIABLE_NAME}
```

### Macros

  With version 1.6 simple macro engine was introduced. 

  Usage:

```
@{{NAME}} 
```
  This will be evauluated by Configus as environmental variable MACRO_NAME

