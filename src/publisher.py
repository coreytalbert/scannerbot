# Publisher
#
# Posts specified files/information onto twitter account @sdscannerbot
#
from os import environ
from subprocess import call
import sys
from dotenv import load_dotenv
import tweepy
import requests


load_dotenv("/home/corey/scannerbot/secret/testscannerbotkeys.env")
client = tweepy.Client(wait_on_rate_limit=True,
                       bearer_token=environ["BEARER_TOKEN"],
                       consumer_key=environ["API_KEY"],
                       consumer_secret=environ["API_SECRET"],
                       access_token=environ["ACCESS_TOKEN"],
                       access_token_secret=environ["ACCESS_TOKEN_SECRET"],
                       return_type=requests.Response)

def read_file(filename):
    with open(filename, 'r') as f:
        text = f.read()
    return text


def tweet(message: str):
        response = client.create_tweet(text=message)
        print(response.headers)
        

def main():
    try:
        if (len(sys.argv) < 2):
            print("No file name given.")
            exit()
        
        message = read_file(sys.argv[1])
        tweet(message)
    except KeyboardInterrupt:
        exit()

if __name__ == "__main__":
    main()