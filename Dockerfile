FROM gcc as builder

RUN git clone https://github.com/libdfp/libdfp.git && \
    cd libdfp && \
    ./configure --enable-decimal-float=bid && \
    make -j4

WORKDIR /src
COPY src /src
RUN gcc \
    -D __STDC_WANT_DEC_FP__ \
    -I /libdfp \
    -L /libdfp -l dfp \
    -o dectest \
    main.c

# -----------------------------------------------------------------------------

FROM debian:stretch

ENV LD_LIBRARY_PATH=/usr/local/lib

COPY --from=builder /src/dectest /usr/local/bin
COPY --from=builder /libdfp/libdfp.so /usr/local/lib/libdfp.so.1

ENTRYPOINT [ "/usr/local/bin/dectest" ]
