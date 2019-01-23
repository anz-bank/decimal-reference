## Usage

This project is not intended for general use. It exists solely to serve as a
reference implementation to validate https://github.com/anz-bank/decimal.

## Install

```bash
$ docker build -t decimal-reference .
```

## Run

```bash
$ docker run -it --rm decimal-reference
$ docker run -it --rm decimal-reference 2 4 x 7 / =
$ echo e pi ^ = pi -100 ^ = | docker run -i --rm decimal-reference -
```
