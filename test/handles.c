#define Model void
#define Entity void

void main()
{
    Model quoteModel = create_model("Quote");
    Model curlyModel = create_model("Curly");

    Entity quote0 = create_entity(quoteModel, 50, 0, 100);
    Entity curly0 = create_entity(curlyModel, 100, 0, 200);
}

