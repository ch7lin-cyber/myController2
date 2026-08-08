import csv



def save_csv(
        filename,
        data):


    with open(
        filename,
        "w",
        newline=""
    ) as f:


        writer = csv.DictWriter(
            f,
            fieldnames=
            [
            "time",
            "SV",
            "PV",
            "PWM"
            ]
        )


        writer.writeheader()



        writer.writerows(data)
