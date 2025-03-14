FROM gcc AS builder

COPY libdfp libdfp
RUN cd /libdfp && \
    ./configure --enable-decimal-float=bid && \
    make -j4

WORKDIR /src
COPY src /src
RUN gcc \
    -D __STDC_WANT_DEC_FP__ \
    -I /libdfp \
    -I /libdfp/dfp \
    -L /libdfp -l dfp \
    -o dectest \
    main.c

# -----------------------------------------------------------------------------

FROM debian

ENV LD_LIBRARY_PATH=/usr/local/lib

COPY --from=builder /src/dectest /usr/local/bin
COPY --from=builder /libdfp/libdfp.so /usr/local/lib/libdfp.so.1

ENTRYPOINT [ "/usr/local/bin/dectest" ]
