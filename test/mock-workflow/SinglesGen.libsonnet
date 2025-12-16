local ev = import 'event_product.libsonnet';

{
  rn222: {
    cpp: "MC_truth_algorithm",
    duration_usec: 39,
    inputs: [ev.event_product("id")],
    outputs: ["MCTruths"]
  },
  ar39: {
    cpp: "MC_truth_algorithm",
    duration_usec: 12410,
    inputs: [ev.event_product("id")],
    outputs: ["MCTruths"]
  },
  cosmicgenerator: {
    cpp: "MC_truth_algorithm",
    duration_usec: 492, # Typical: 4926215
    inputs: [ev.event_product("id")],
    outputs: ["MCTruths"]
  },
  kr85: {
    cpp: "MC_truth_algorithm",
    duration_usec: 1643,
    inputs: [ev.event_product("id")],
    outputs: ["MCTruths"]
  },
  generator: {
    cpp: "three_tuple_algorithm",
    duration_usec: 69616,
    inputs: [ev.event_product("id")],
    outputs: ["MCTruths", "BeamEvents", "beamsims"]
  },
  ar42: {
    cpp: "MC_truth_algorithm",
    duration_usec: 148,
    inputs: [ev.event_product("id")],
    outputs: ["MCTruths"]
  },
}
