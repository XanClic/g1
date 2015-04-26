type = FLIGHT_CONTROL
name = "L2"


function kill_rotation(ship_state)
    states = {}
    thruster_i = 0

    angular_momentum = ship_state.rotational_velocity * ship_state.total_mass

    while ship_state.thrusters[thruster_i] ~= nil do
        thruster = ship_state.thrusters[thruster_i]
        state = 0.0

        if thruster.type == RCS then
            force = crossp(thruster.relative_position, thruster.force)
            states[thruster_i] = -10.0 * dotp(force, angular_momentum) / (force:length() ^ 2.0)
        end

        thruster_i = thruster_i + 1
    end

    return states
end


function flight_control(ship_state, input)
    if input.kill_rotation then
        return kill_rotation(ship_state)
    end

    return nil
end
