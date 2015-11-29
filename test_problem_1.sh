#!/bin/sh
./MinimalFEM test1.inp test1.out
python scripts/PostProcess.py test1.inp test1.out