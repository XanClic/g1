type = FLIGHT_CONTROL
name = "L1"


function flight_control(ship_state, input, interval)
    states = {}
    thruster_i = 0

    while ship_state.thrusters[thruster_i] ~= nil do
        thruster = ship_state.thrusters[thruster_i]
        state = 0.0

        if thruster.type == THRUSTER_MAIN then
            if thruster.force.z > 0.0 then
                state = -input.main_engine
            else
                state =  input.main_engine
            end
        elseif thruster.type == THRUSTER_RCS then
            -- This doesn't work with arbitrary thruster directions. Who cares.
            state = dotp(thruster.force, input.strafe) / thruster.force:length()

            torque = crossp(thruster.relative_position, thruster.force)
            if torque:length() > 0.0 then
                state = state + dotp(torque, input.rotate) / torque:length()
            end
        end

        states[thruster_i] = state

        thruster_i = thruster_i + 1
    end

    return states
end
