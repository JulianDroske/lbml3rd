#!/bin/sh
# Generate fbpad fonts

FP="."
# OP="-h34 -w19"
# SZ="18h135v135"
OP="-h26 -w14"
SZ="18h100v100"
./mkfn_ft $OP $FP/GWMonospace.ttf:$SZ	>ar.tf
./mkfn_ft $OP $FP/GWMonospaceOblique.ttf:$SZ	>ai.tf
./mkfn_ft $OP $FP/GWMonospaceBold.ttf:$SZ	>ab.tf

