#!/bin/sh
./MinimalFEM test2.inp test2.out
python scripts/PostProcess.py test2.inp test2.out