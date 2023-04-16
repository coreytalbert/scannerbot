# Transcriber
#
# Receives any audio file, sending it through the speech-to-text
# library/API, and returns the output as a text file.
#
# Before using OpenAI's Whisper, it must be installed on the development
# machine.
# https://github.com/openai/whisper#setup
import sys
import whisper

class Transcriber:
    def __init__(self, input_path, model="base"):
        self.input_path = input_path
        # initialize whisper
        self.model = whisper.load_model(model)
        # perform transcription
        self.output_text = self.model.transcribe(input_path, verbose=True, language="en")

    def __str__(self):
        #return f"Transcriber [path={self.input_path}, text={self.output_text}]"
        return self.output_text.get("text")


def main():
    if (len(sys.argv) < 2):
        print("Missing filename.\nUsage:\n\tmain.py filename")
        sys.exit()
    elif (len(sys.argv) > 3):
        print("Too many arguments.")
        sys.exit()
    filename = sys.argv[1]
    
    t = Transcriber(filename, "large")
    print(t)


if __name__ == "__main__":
    main()