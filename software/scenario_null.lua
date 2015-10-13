type = SCENARIO
name = "null"


function initialize()
    ps = player_ship()

    ps:set_position(1.40673537711, 0.495382619358, 300e3)
    ps:set_bearing(0)
    ps:set_velocity(0, 0, 7730.811)

    ns = spawn_ship("mumeifune")
    ns:set_position(1.407, 0.49530, 297.5e3)
    ns:set_bearing(0)
    ns:set_velocity(0, 20, 7720)

    ns = spawn_ship("mumeifune")
    ns:set_position(1.407, 0.49535, 297.5e3)
    ns:set_bearing(0)
    ns:set_velocity(0, 20, 7720)

    ns = spawn_ship("mumeifune")
    ns:set_position(1.407, 0.49540, 297.5e3)
    ns:set_bearing(0)
    ns:set_velocity(0, 20, 7720)

    enable_player_physics(true)
end
