from fuzzy_controller import *


from fuzzy_sim import *


from logger import *


from analyze import *



controller = (
    FuzzyController()
)



sim = (
    FuzzySimulation(
        controller
    )
)



data = sim.run(
        175,
        120
)



save_csv(
    "step175.csv",
    data
)



result = analyze(
        data,
        175
)


print(result)
