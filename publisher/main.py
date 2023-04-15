# Publisher
#
# Posts specified files/information onto twitter account @sdscannerbot
#

import tweepy
import keys

def api():
	auth = tweepy.OAuthHandler(keys.api_key, keys.api_secret)
	auth.set_access_token(keys.access_token, keys.access_token_secret)
	
	return tweepy.API(auth)

def read_file(filename):
    with open(filename, 'r') as f:
        text = f.read()
    return text


def tweet(api: tweepy.API, message: str):
	api.update_status(message)

	print("Tweeted success!")

#----------------------------------

if __name__ == "__main__":
	api = api()
	tweet(api, read_file("test_tweet.txt")