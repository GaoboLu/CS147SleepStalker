import streamlit as st
import plotly.graph_objects as go
import time
import json
import datetime
import random
from streamlit_autorefresh import st_autorefresh



def dictToLists():
    Times = []
    Humidity = []
    Temperature = []
    ppm = []
    Volts = []
    Light = []
    Movement = []
    with open('data.json', "r") as json_data_file:
        data = json.load(json_data_file)
    for time, readings in sorted(data.items(), key=lambda x: x[0]):
        Times.append(datetime.datetime.fromtimestamp(float(time)))
        Humidity.append(float(readings[0]))
        Temperature.append(float(readings[1]))
        tmpPPM = float(readings[2])/1000000
        ppm.append(tmpPPM if tmpPPM < 100 else 100)
        Volts.append(float(readings[5]))
        Light.append(int(readings[3])/10)
        Movement.append(int(readings[4])*100)
    return Times, Humidity, Temperature, ppm, Volts, Light, Movement



# Function to create the plot
def plot_multiple_data():
    Times, Humidity, Temperature, ppm, Volts, Light, Movement = dictToLists()
    fig = go.Figure()
    legend = ['Humidity', 'Temperature', 'Pollutants', 'Noise', 'Light', 'Movement']
    fig.add_trace(
        go.Scatter(x = Times, y = Humidity, mode = "lines+markers", name = legend[0]))  # Humidity
    fig.add_trace(
        go.Scatter(x = Times, y = Temperature, mode = "lines+markers",name = legend[1]))  # Temperature
    fig.add_trace(
        go.Scatter(x = Times, y = ppm, mode = "lines+markers", name = legend[2]))  # PPM
    fig.add_trace(
        go.Scatter(x = Times, y = Volts, mode = "lines+markers", name = legend[3]))  # Volts
    fig.add_trace(
        go.Scatter(x = Times, y = Light, mode = "lines+markers", name = legend[4]))  # Light
    fig.add_trace(
        go.Scatter(x = Times, y = Movement, mode = "lines+markers", name = legend[5]))  # Movement
    
    fig.update_xaxes(title_text = "Time (seconds)")
    fig.update_yaxes(title_text = "Value")

    return fig


# Main dashboard function
def run_dashboard():
    # Auto-refresh every 10 seconds
    st_autorefresh(interval = 11 * 1000, key = "datarefresh")

    st.write("CS 147 Sleep Stalker")
    some_plotly_figure = plot_multiple_data()
    st.write(some_plotly_figure)
    # st.plotly_chart(some_plotly_figure)


if __name__ == "__main__":
    run_dashboard()