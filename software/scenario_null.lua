type = SCENARIO
name = "null"


function initialize()
    ps = player_ship()

    ps:set_position(1.40673537711, 0.495382619358, 300000)
    ps:set_bearing(0)

    ns = spawn_ship("mumeifune")
    ns:set_position(1.407, 0.49530, 297500)
    ns:set_bearing(0)
    ns:set_velocity(0, 20, 7720)

    ns = spawn_ship("mumeifune")
    ns:set_position(1.407, 0.49535, 297500)
    ns:set_bearing(0)
    ns:set_velocity(0, 20, 7720)

    ns = spawn_ship("mumeifune")
    ns:set_position(1.407, 0.49540, 297500)
    ns:set_bearing(0)
    ns:set_velocity(0, 20, 7720)
end


function step(interval, ship_state)
    enable_player_physics(true)

    ship_state.velocity.x = ship_state.forward.x * 7730.811
    ship_state.velocity.y = ship_state.forward.y * 7730.811
    ship_state.velocity.z = ship_state.forward.z * 7730.811

    return ship_state
end
