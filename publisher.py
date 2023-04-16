# Publisher
#
# Posts specified files/information onto twitter account @sdscannerbot
#
import tweepy
import keys

client = tweepy.Client(consumer_key=keys.api_key,
                       consumer_secret=keys.api_secret,
                       access_token=keys.access_token,
                       access_token_secret=keys.access_token_secret)

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
	tweet("test tweet")