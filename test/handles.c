// Test "handles" to internal engine types. Since ChronoScript is currently standalone, the current "engine types"
// are dummy types, very loosely inspired by engine types in OpenBOR.

#define Model void
#define Entity void

void main()
{
    Model quoteModel = create_model("Quote");
    Model curlyModel = create_model("Curly");

    Entity quote0 = create_entity(quoteModel, 50, 0, 100);
    Entity curly0 = create_entity(curlyModel, 100, 0, 200);

    log("Quote's model name is " + quoteModel.name);
    log("Curly's model name is " + curly0.model.name);

    quote0.move(0, 15.5, 0);
    log("Quote's position: x=" + quote0.pos_x + " y=" + quote0.pos_y + " z=" + quote0.pos_z);
    quote0.vy += 1.5;
    log("Quote's velocity: x=" + quote0.vx + " y=" + quote0.vy + " z=" + quote0.vz);
}

