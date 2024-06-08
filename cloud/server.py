from flask import Flask
from flask import request
import datetime
import time
import json
from pathlib import Path

TimeDict = {}

def updateDict(Canister):
    curTime = time.time()
    capsules = Canister.strip(';').split(';')
    for item in capsules:
        cap = item.strip("()").split(',')
        itemTime = curTime - int(cap[6])/1000
        TimeDict.update({itemTime: cap[:6]})

def saveToJson():
    database = Path('data.json')
    try:
        database.touch(exist_ok=False)
    except FileExistsError:
        with open(database, "r") as json_data_file:
            data = json.load(json_data_file)
        data.update(TimeDict)
    else:
        data = TimeDict
    with open(database, "w") as outfile: 
        json.dump(data, outfile)


app = Flask(__name__)
@app.route("/", methods=['GET', 'POST'])
def update():
    print(request.args.get("var"))

    updateDict(request.args.get("var"))
    if len(TimeDict) > 1:
        saveToJson()
        TimeDict.clear()
    return "We received value"