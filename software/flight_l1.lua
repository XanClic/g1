type = FLIGHT_CONTROL
name = "L1"


function flight_control(ship_state, input, interval)
    states = {}
    thruster_i = 0

    while ship_state.thrusters[thruster_i] ~= nil do
        thruster = ship_state.thrusters[thruster_i]
        state = 0.0

        if thruster.type == MAIN then
            if thruster.general_direction == BACKWARD then
                state = -input.main_engine
            else
                state =  input.main_engine
            end
        elseif thruster.type == RCS then
            if thruster.general_direction == RIGHT or thruster.general_direction == LEFT then
                if thruster.general_direction == RIGHT then
                    sign =  1.0
                else
                    sign = -1.0
                end

                state = sign * input.strafe.x
                if thruster.general_position == TOP then
                    state = state +  sign * input.rotate.z
                elseif thruster.general_position == BOTTOM then
                    state = state + -sign * input.rotate.z
                elseif thruster.general_position == FRONT then
                    state = state +  sign * input.rotate.y
                elseif thruster.general_position == BACK then
                    state = state + -sign * input.rotate.y
                end
            elseif thruster.general_direction == UP or thruster.general_direction == DOWN then
                if thruster.general_direction == UP then
                    sign =  1.0
                else
                    sign = -1.0
                end

                state = sign * input.strafe.y;
                if thruster.general_position == RIGHT then
                    state = state + -sign * input.rotate.z;
                elseif (thruster.general_position == LEFT) then
                    state = state +  sign * input.rotate.z;
                elseif (thruster.general_position == FRONT) then
                    state = state +  sign * input.rotate.x;
                elseif (thruster.general_position == BACK) then
                    state = state + -sign * input.rotate.x;
                end
            elseif thruster.general_direction == FORWARD or thruster.general_direction == BACKWARD then
                if thruster.general_direction == FORWARD then
                    sign =  1.0
                else
                    sign = -1.0
                end

                state = sign * input.strafe.z;
                if thruster.general_position == RIGHT then
                    state = state + -sign * input.rotate.y;
                elseif thruster.general_position == LEFT then
                    state = state +  sign * input.rotate.y;
                elseif thruster.general_position == TOP then
                    state = state + -sign * input.rotate.x;
                elseif thruster.general_position == BOTTOM then
                    state = state +  sign * input.rotate.x;
                end
            end
        end

        states[thruster_i] = state

        thruster_i = thruster_i + 1
    end

    return states
end
