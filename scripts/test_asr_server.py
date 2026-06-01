import argparse
from openai import OpenAI

def get_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ip", type=str, required=True)
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--audio", "-a", type=str, required=True)
    parser.add_argument("--model", "-m", type=str, required=True, choices=["sensevoice", "whisper_tiny", "whisper_base", "whisper_small", "whisper_turbo", "gpt-4o-transcribe", "gpt-4o-mini-transcribe", "whisper-1"])
    parser.add_argument("--language", "-l", type=str, default=None)
    parser.add_argument("--api-key", type=str, default="dummy_key")
    parser.add_argument("--response-format", choices=["json", "text", "verbose_json"], default="json")
    return parser.parse_args()

args = get_args()
client = OpenAI(
    base_url=f'http://{args.ip}:{args.port}/v1',
    api_key=args.api_key
)
with open(args.audio, "rb") as audio_file:
    kwargs = {
        "model": args.model,
        "file": audio_file,
        "response_format": args.response_format,
    }
    if args.language:
        kwargs["language"] = args.language

    transcription = client.audio.transcriptions.create(**kwargs)

if args.response_format == "text":
    print(transcription)
else:
    print(f"audio: {args.audio}  text: {transcription.text}")
