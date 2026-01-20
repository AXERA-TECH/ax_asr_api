#!/bin/bash
set -x

AX650_PATH="models-ax650"
AX630C_PATH="models-ax630c"
WHISPER_PATH="whisper"
SENSEVOICE_PATH="sensevoice"

mkdir -p $AX650_PATH
mkdir -p $AX630C_PATH

mkdir -p $AX650_PATH/$WHISPER_PATH
mkdir -p $AX650_PATH/$SENSEVOICE_PATH

mkdir -p $AX630C_PATH/$WHISPER_PATH
mkdir -p $AX630C_PATH/$SENSEVOICE_PATH

export HF_ENDPOINT=https://hf-mirror.com

# sensevoice
hf download AXERA-TECH/SenseVoice --local-dir SenseVoice
cp -rf SenseVoice/sensevoice_ax650/* $AX650_PATH/$SENSEVOICE_PATH
cp -rf SenseVoice/sensevoice_ax630c/* $AX630C_PATH/$SENSEVOICE_PATH
rm -rf SenseVoice

# whisper
hf download AXERA-TECH/Whisper --local-dir Whisper
cp -rf Whisper/models-ax650/* $AX650_PATH/$WHISPER_PATH
cp -rf Whisper/models-ax630c/* $AX630C_PATH/$WHISPER_PATH
rm -rf Whisper

echo "ALL DONE"