from flask import Flask
from flask import request

app = Flask(__name__)

@app.route("/")
def get_speed():
	with open("/var/www/html/speed.txt", "w") as f_speed:
		speed = request.args.get("speed")
		print("Speed: " + speed)
		print(speed, file=f_speed)
#	with open("/var/www/html/vid.txt", "w") as f_vid:
#		vid = request.args.get("vid")
#		print("VID: " + vid)
#		print(vid, file=f_vid)
	return "Received speed and vid\n";
