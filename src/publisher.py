# Publisher
#
# Posts specified files/information onto twitter account @sdscannerbot
#
from os import environ
from subprocess import call
from dotenv import load_dotenv
import tweepy

load_dotenv("../secret/keys.env")
client = tweepy.Client(consumer_key=environ["API_KEY"],
                       consumer_secret=environ["API_SECRET"],
                       access_token=environ["ACCESS_TOKEN"],
                       access_token_secret=environ["ACCESS_TOKEN_SECRET"])

def read_file(filename):
    with open(filename, 'r') as f:
        text = f.read()
    return text


def tweet(message: str):
	response = client.create_tweet(text=message)
	print(response)

#----------------------------------

if __name__ == "__main__":
	#api = api()
	tweet("test tweet 5")