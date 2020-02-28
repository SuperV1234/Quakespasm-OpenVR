  Con_Printf("has handtouch\n");


        Con_Printf("c %d>%d %d>%d %d>%d %d<%d %d<%d %d<%d\n", handposmin[0],
            target->v.absmax[0], handposmin[1], target->v.absmax[1],
            handposmin[2], target->v.absmax[2], handposmax[0],
            target->v.absmin[0], handposmax[1], target->v.absmin[1],
            handposmax[2], target->v.absmin[2]);



// TODO VR: this kinda works in E1M1 if put inside FlyMove
// TODO VR: test, what is this one for? How does it differ from world.cpp?
    auto doHandTouch = [&](vec3_t handpos, vec3_t handrot, int type) {
        vec3_t fwd, right, up, end;
        AngleVectors(handrot, fwd, right, up);
        fwd[0] *= 1.f;
        fwd[1] *= 1.f;
        fwd[2] *= 1.f;

        VectorCopy(handpos, end);
        VectorAdd(end, fwd, end);

        vec3_t mins{-2, -2, -2};
        vec3_t maxs{2, 2, 2};
        // trace_t trace = SV_Move(handpos, mins, maxs, end, type, ent);
        trace_t trace =
            SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, end, type, ent);

        // vec3_t impact;
        // trace_t trace = TraceLine(cl.handpos[1], end, impact);
        // SV_ClipMoveToEntity(sv.edicts, start, mins, maxs, end);

        /* if(trace.allsolid)
         { // entity is trapped in another solid
             // Con_Printf("all solid\n");
             return;
         }


         if(trace.fraction == 1)
         { // moved the entire distance
             // Con_Printf("entire dist\n");
             return;
         }*/

        if(!trace.ent)
        {
            // Con_Printf("no ent\n");
            return;
        }

        vec3_t aMin, aMax, bMin, bMax;
        VectorAdd(trace.ent->v.origin, trace.ent->v.mins, aMin);
        VectorAdd(trace.ent->v.origin, trace.ent->v.maxs, aMax);
        VectorAdd(handpos, mins, bMin);
        VectorAdd(handpos, maxs, bMax);

        if(quake::util::boxIntersection(aMin, aMax, bMin, bMax))
        {
            SV_Impact(ent, trace.ent, &entvars_t::handtouch);
        }
    };

    doHandTouch(ent->v.handpos, ent->v.handrot, MOVE_NORMAL);
    doHandTouch(ent->v.offhandpos, ent->v.offhandrot, MOVE_NORMAL);
