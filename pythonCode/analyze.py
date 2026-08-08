def analyze(data,SV):


    pv=[
        x["PV"]
        for x in data
    ]


    overshoot = (
        max(pv)-SV
    )



    steady_error = (
        SV-pv[-1]
    )



    rise_time=None



    for x in data:


        if x["PV"]>=0.9*SV:

            rise_time=x["time"]

            break



    return {


    "Overshoot":
        overshoot,


    "RiseTime":
        rise_time,


    "SteadyError":
        steady_error


    }
