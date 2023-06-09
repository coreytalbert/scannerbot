# Transcriber
#
# Receives any audio file, sending it through the speech-to-text
# library/API, and returns the output as a text file.
import sys
import whisper

def write_file(audioPath, transcription):
    audioPathDirs = audioPath.split('/')
    transcriptPath = "/home/corey/scannerbot/transcripts/" + audioPathDirs[len(audioPathDirs)-1].split('.')[0] + ".txt"
    with open(transcriptPath, 'w') as file:
        file.write(transcription)

class Transcriber:
    def __init__(self, input_path, model="large"):
        self.input_path = input_path
        # initialize whisper
        self.model = whisper.load_model(model)
        # perform transcription
        self.output_text = self.model.transcribe(input_path, verbose=False)

    def __str__(self):
        return self.output_text.get("text")


def main():
    try:
        if (len(sys.argv) < 2):
            print("Missing filename.\nUsage:\n\ttranscriber.py filename")
            sys.exit()
        elif (len(sys.argv) > 3):
            print("Too many arguments.")
            sys.exit()
        
        audioFilePath = sys.argv[1]
        model = "tiny"
        if(len(sys.argv) == 3):
            model=sys.argv[2]
        
        t = Transcriber(audioFilePath, model)
        write_file(audioFilePath, t.output_text.get("text"))
    except KeyboardInterrupt:
        exit()

if __name__ == "__main__":
    main()