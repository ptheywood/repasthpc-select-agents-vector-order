#include <string>
#include <memory>
#include <vector>
#include <numeric>
#include <boost/mpi.hpp>
#include "repast_hpc/RepastProcess.h"
#include "repast_hpc/initialize_random.h"
#include "repast_hpc/AgentId.h"
#include "repast_hpc/SharedContext.h"
#include "repast_hpc/Schedule.h"

/**
 * Class representing agents, which simply have an Id.
 * operator delete is overloaded and emptied to allow use of placement new, this is a leak but for the MWE this is sufficient to demonstrate the behaviour.
 */
class Agent {
public:
    Agent(repast::AgentId id) : _id(id) { }
    ~Agent() { }
    virtual repast::AgentId& getId(){ return _id; }
    virtual const repast::AgentId& getId() const { return _id; }
    repast::AgentId _id;
    // Overloaded operator delete to prevent segfaults from placement new.
    static void operator delete(void* ptr, std::size_t sz) {
        // Do nothing to allow use of palcement new. Good enouh for this MWE but not ideal.
    };
};

/** 
 * Class for a simpler RepastHPC model.
 * On `init`, it allocates a number of agents using one of 3 allocation orderings, printing ID's and memory addresses in order
 * A single step is scheduled, during which selectAgents is used to get a randomly order vector of all agents. The vector contents are printed to stdout in order.
 */
class Model {
public:
    // Maximum number of agents if using placement new
    static constexpr size_t MAX_AGENTS = 4096;
    // repast properties config 
    repast::Properties * props;
    // repast context
    repast::SharedContext<Agent> context;
    // Aligned storage for a fixed maximum number of agents 
    std::aligned_storage_t<sizeof(Agent), alignof(Agent)> buffer[Model::MAX_AGENTS];
    /**
     * ctor, loads properties file and prints to stdout
     */
	Model(std::string propsFile, int argc, char** argv, boost::mpi::communicator* comm) :
    props(nullptr), context(comm) {
        this->props = new repast::Properties(propsFile, argc, argv, comm);
        printf("random.seed: %d\n", repast::strToInt(props->getProperty("random.seed")));
        printf("count.of.agents: %d\n", repast::strToInt(props->getProperty("count.of.agents")));
        printf("count.to.select: %d\n", repast::strToInt(props->getProperty("count.to.select")));
        printf("order.placementnew: %d\n", repast::strToInt(props->getProperty("order.placementnew")));
        printf("order.reverse: %d\n", repast::strToInt(props->getProperty("order.reverse")));
    }
    /**
     * dtor
     */
	~Model() {
        if(props != nullptr) {
            delete this->props;
        }
    }

    /**
     * Initialise the simple model:
     * + Seeds RNG
     * + allocates agents using new or placement new (in asc or desc order), outputting to stdout
     */ 
    void init() {
        std::cout << "Model::init" << std::endl;
        boost::mpi::communicator world;
        // Initialise RNG with seed from props.
        repast::initializeRandom(*this->props, &world);
        const int rank = repast::RepastProcess::instance()->rank();
        const int count = repast::strToInt(props->getProperty("count.of.agents"));
        const bool order_placementnew = repast::strToInt(props->getProperty("order.placementnew"));
        const bool order_reverse = repast::strToInt(props->getProperty("order.reverse"));

        // Initialise a vector of size_t with [0, count)
        std::vector<size_t> ptrOrder(count);
        std::iota(ptrOrder.begin(), ptrOrder.end(), 0);

        // Conditionally reverse the ptrOrder
        if(order_reverse) {
            std::reverse(ptrOrder.begin(), ptrOrder.end());
        }
        for(int i = 0; i < count; i++){
            repast::AgentId id(i, rank, 0);
            id.currentRank(rank);

            Agent* agent = nullptr;
            // allocate agents using new or placement new depending on model config
            if(!order_placementnew) {
                agent = new Agent(id);
            } else {
                // Use palcement new with explicit ordering
                if(ptrOrder[i] <= Model::MAX_AGENTS) {
                    agent = new(&buffer[ptrOrder[i]]) Agent(id);
                } else {
                    std::cerr << "Error: more agents requirest than " << MAX_AGENTS << " " << __FILE__  << "::" << __LINE__ <<std::endl;
                    exit(EXIT_FAILURE);
                }
            }
            std::cout << agent->getId() << " initalised at " << static_cast<void*>(agent) << std::endl;
            context.addAgent(agent);
        }
    }
    /**
     * Schedule a single tick, which calls selectAndPrint once
     */
    void initSchedule(repast::ScheduleRunner& runner) { 
        runner.scheduleEvent(1, 1, repast::Schedule::FunctorPtr(new repast::MethodFunctor<Model> (this, &Model::selectAndPrint)));
        runner.scheduleStop(1);
    }
    /**
     * Use context::selectAgents to get a vector of all agents.
     * Output agent IDs and pointers from the vector to stdout
     */
    void selectAndPrint() {
        const int count = repast::strToInt(props->getProperty("count.of.agents"));
        const int countToSelect = repast::strToInt(props->getProperty("count.to.select"));
        std::cout << "selectAndPrint " << countToSelect << "/" << count << std::endl;
        std::vector<Agent*> agents = {};
        this->context.selectAgents(countToSelect, agents);
        for (Agent* const agent : agents ) {
            std::cout << agent->getId() << " initalised at " << static_cast<void*>(agent) << std::endl;
        }
    }
};

/**
 * Print usage, expecting 2 props files
 */
void usage(int argc, char** argv) {
    std::cerr << "usage: " << argv[0] << " <config.props> <model.props>" << std::endl;
}

/**
 * Main method including CLI parsing, model setup and scheduling.
 */
int main(int argc, char ** argv) {
    boost::mpi::environment env(argc, argv);
    boost::mpi::communicator world;

    if (argc != 3) {
        if(world.rank() == 0) {
            usage(argc, argv);
        }
        return EXIT_FAILURE;
    }
    std::string configPath = argv[1];
    std::string propsPath = argv[2];

    repast::RepastProcess::init(configPath, &world);
    repast::Properties props(propsPath, argc, argv, &world);

    Model model = Model(propsPath, argc, argv, &world);
    model.init();

    repast::ScheduleRunner& runner = repast::RepastProcess::instance()->getScheduleRunner();
    model.initSchedule(runner);

    runner.run();
    world.barrier();

	repast::RepastProcess::instance()->done();
    return EXIT_SUCCESS;
}