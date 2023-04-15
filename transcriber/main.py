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
    def __init__(self, input_path):
        self.input_path = input_path
        # initialize whisper
        self.model = whisper.load_model("base")
        # perform transcription
        self.output_text = self.model.transcribe(input_path)

    def __str__(self):
        #return f"Transcriber [path={self.input_path}, text={self.output_text}]"
        return self.output_text.get("text")

# end class Transcriber

# ========== start demo ==========


def main():
    if (len(sys.argv) < 2):
        print("Missing filename.\nUsage:\n\tmain.py filename")
        sys.exit()
    elif (len(sys.argv) > 2):
        print("Too many arguments.")
        sys.exit()
    filename = sys.argv[1]
    
    t = Transcriber(filename)
    print(t)


if __name__ == "__main__":
    main()

# ========== end demo ==========