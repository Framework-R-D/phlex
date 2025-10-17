{
  rn222: {
    plugin: "MC_truth_algorithm",
    duration_usec: 39,
    inputs: ["id"],
    outputs: ["MCTruths"]
  },
  ar39: {
    plugin: "MC_truth_algorithm",
    duration_usec: 12410,
    inputs: ["id"],
    outputs: ["MCTruths"]
  },
  cosmicgenerator: {
    plugin: "MC_truth_algorithm",
    duration_usec: 492, # Typical: 4926215
    inputs: ["id"],
    outputs: ["MCTruths"]
  },
  kr85: {
    plugin: "MC_truth_algorithm",
    duration_usec: 1643,
    inputs: ["id"],
    outputs: ["MCTruths"]
  },
  generator: {
    plugin: "three_tuple_algorithm",
    duration_usec: 69616,
    inputs: ["id"],
    outputs: ["MCTruths", "BeamEvents", "beamsims"]
  },
  ar42: {
    plugin: "MC_truth_algorithm",
    duration_usec: 148,
    inputs: ["id"],
    outputs: ["MCTruths"]
  },
}
