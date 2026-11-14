#include <Singularity/SingularityEngineRuntime.h>

#include <exception>
#include <iostream>

int main()
{
    try
    {
        Singularity::SingularityEngineRuntime engine;
        while (!engine.shouldClose())
        {
            engine.runFrame();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
