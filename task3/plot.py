#!/usr/bin/python3
import numpy as np
import pandas as pd
import plotly.express as px
import plotly.graph_objects as go

checkpoint = pd.read_csv('results/checkpoint.csv')
checkpoint_fsync = pd.read_csv('results/checkpoint_fsync.csv')
checkpoint_optimized = pd.read_csv('results/checkpoint_optimized.csv')

def total_runtime_plot(df, name):
    fig = go.Figure()
    fig.add_trace(go.Scatter(x=df["threads"], y=df["wallclock"], mode='lines+markers'))
    fig.update_layout(
        # title=f"{name} - application runtime",
        yaxis_title="runtime in s",
        xaxis_title="thread count",
        font=dict(
            size=18
        )
    )
    fig.write_image(f"plots/{name}_time.svg")

def relative_runtime_plot(df1, df2, df3, name):
    fig = go.Figure()
    fig.add_trace(go.Scatter(x=df1["threads"], y=df1["wallclock"]/df1["iterations"], mode='lines+markers', name="checkpoint"))
    fig.add_trace(go.Scatter(x=df2["threads"], y=df2["wallclock"]/df2["iterations"], mode='lines+markers', name="checkpoint_fsync"))
    fig.add_trace(go.Scatter(x=df3["threads"], y=df3["wallclock"]/df3["iterations"], mode='lines+markers', name="checkpoint_optimized"))
    fig.update_layout(
        # title=f"{name} - relative application runtime",
        yaxis_title="seconds per iteration",
        xaxis_title="thread count",
        font=dict(
            size=18
        )
    )
    fig.write_image("plots/relative_time.svg")

def bandwidth_plot(df1, df2, df3, name):
    fig = go.Figure()
    fig.add_trace(go.Scatter(x=df1["threads"], y=df1["io_bytes"] / 1024 / 1024 / df1["wallclock"], mode='lines+markers', name="checkpoint"))
    fig.add_trace(go.Scatter(x=df2["threads"], y=df2["io_bytes"] / 1024 / 1024 / df2["wallclock"], mode='lines+markers', name="checkpoint_fsync"))
    fig.add_trace(go.Scatter(x=df3["threads"], y=df3["io_bytes"] / 1024 / 1024 / df3["wallclock"], mode='lines+markers', name="checkpoint_optimized"))
    fig.update_layout(
        # title=f"{name} - average throughput",
        yaxis_title="throughput in MiB/s",
        xaxis_title="thread count",
        font=dict(
            size=18
        )
    )
    fig.write_image("plots/bandwidth.svg")

def idle_chart(df1, df2, df3, name):
    versions = ['checkpoint', 'checkpoint_fsync', 'checkpoint_optimized']
    fsync = [df1["fsync_time"].mean(), df2["fsync_time"].mean(), df3["fsync_time"].mean()]
    other_io = [(df1["io_time"] - df1["fsync_time"]).mean(), (df2["io_time"] - df2["fsync_time"]).mean(), (df3["io_time"] - df3["fsync_time"]).mean()]
    fig = go.Figure()
    fig.add_trace(go.Bar(name="fsync", x=versions, y=fsync))
    fig.add_trace(go.Bar(name="other_io", x=versions, y=other_io))
    fig.update_layout(
        # title=f"{name} - average io time",
        yaxis_title="time spend in s",
        barmode="stack",
        font=dict(
            size=18
        )
    )
    fig.write_image("plots/iotime.svg")

def iops(df1, df2, df3, name):
    fig = go.Figure()
    fig.add_trace(go.Scatter(x=df1["threads"], y=df1["iop"] / df1["wallclock"], mode='lines+markers', name="checkpoint"))
    fig.add_trace(go.Scatter(x=df2["threads"], y=df2["iop"] / df2["wallclock"], mode='lines+markers', name="checkpoint_fsync"))
    fig.add_trace(go.Scatter(x=df3["threads"], y=df3["iop"] / df3["wallclock"], mode='lines+markers', name="checkpoint_optimized"))
    fig.update_layout(
        # title=f"{name} - io operations",
        yaxis_title="io operation per second",
        xaxis_title="thread count",
        font=dict(
            size=18
        )
    )
    fig.write_image("plots/iops.svg")

def runtime_to_io(df, name):
    fig = go.Figure()
    fig.add_trace(go.Bar(name="fsync", x=df["threads"], y=df["fsync_time"]))
    fig.add_trace(go.Bar(name="other io", x=df["threads"], y=df["io_time"] - df["fsync_time"]))
    fig.add_trace(go.Bar(name="total runtime", x=df["threads"], y=df["wallclock"]))
    fig.update_layout(
        # title=f"{name} - runtime to io-time",
        yaxis_title="time spend in seconds",
        xaxis_title="thread count",
        font=dict(
            size=18
        )
    )
    fig.write_image(f"plots/time_{name}.svg")


total_runtime_plot(checkpoint, "checkpoint")
total_runtime_plot(checkpoint_fsync, "checkpoint_fsync")
total_runtime_plot(checkpoint_optimized, "checkpoint_optimized")

relative_runtime_plot(checkpoint, checkpoint_fsync, checkpoint_optimized, "all")

bandwidth_plot(checkpoint, checkpoint_fsync, checkpoint_optimized, "all")
idle_chart(checkpoint, checkpoint_fsync, checkpoint_optimized, "all")
iops(checkpoint, checkpoint_fsync, checkpoint_optimized, "all")

runtime_to_io(checkpoint, "checkpoint")
runtime_to_io(checkpoint_fsync, "checkpoint_fsync")
runtime_to_io(checkpoint_optimized, "checkpoint_optimized")
