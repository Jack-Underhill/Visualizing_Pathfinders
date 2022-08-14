#include "application.h"

namespace PFSim {

#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)

    Application::Application() 
    {
        m_IsInstantPathUpdatingEnabled = false;
        m_IsValidToInstantPathUpdate = false;

        m_Window = new Window();
        m_Window->setEventCallback(BIND_EVENT_FN(onEvent));
        
        m_Graph = new MazeGraph();

        m_AnimationTimer = new AnimationTimer();

        SimulateGeneration gen(m_Graph, m_Window, m_AnimationTimer);
        gen.run(Open);
    }

    Application::~Application() 
    {
        delete m_Graph;
        delete m_Window;
        delete m_AnimationTimer;
    }

    void Application::run() 
    {
        //inifinitely keeping the project up
        while(m_Window->isRunning()) {}
    }

    void Application::onEvent(Event& e) 
    {
        // std::cout << "Event Fired: " << e << std::endl;

        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<MouseButtonPressedEvent>(BIND_EVENT_FN(onMousePressedEvent));
        dispatcher.Dispatch<MouseButtonReleasedEvent>(BIND_EVENT_FN(onMouseReleasedEvent));
        dispatcher.Dispatch<MouseMovedEvent>(BIND_EVENT_FN(onMouseMovedEvent));

        // refactor buttonEvents to just be 1 button class instead of these 4. ///////////////////////////////////////////////////////////////////////////////////////////
        // use buttonPressedEvent and then use button codes to decipher which.
        // onButtonPressEvent will be the function given to the dispatcher and that function will decipher its button code.
        dispatcher.Dispatch<UpdateCheckpointEvent>(BIND_EVENT_FN(onCheckpointEvent));
        dispatcher.Dispatch<UpdatePathfinderEvent>(BIND_EVENT_FN(onPathfinderEvent));
        dispatcher.Dispatch<UpdateGeneratorEvent>(BIND_EVENT_FN(onGeneratorEvent));
        dispatcher.Dispatch<WallCellClearEvent>(BIND_EVENT_FN(onWallCellClearEvent));

        
        dispatcher.Dispatch<SliderMovedEvent>(BIND_EVENT_FN(onSliderEvent));
        
        //refactor this into a checkbox enabled event and decipher from checkboxcodes
        //refactor out eventcategories if found redundant rn
        dispatcher.Dispatch<InstantPathUpdatingEvent>(BIND_EVENT_FN(onInstantPathUpdatingEvent));
    }

    bool Application::onCheckpointEvent(UpdateCheckpointEvent& e)
    {
        if(!m_Graph->isReadyForSimulation())
        {
            SimulatePathfinding pf(m_Graph, m_Window, m_AnimationTimer);
            pf.reset();
        }

        if(e.getButtonCode() == ButtonCode::cp_Add) 
        {
            updateCPButtons(true);
            
            m_Window->getSimulationDisplay()->updateMazeNode( m_Graph->addCheckpoint(), m_Graph->getCellSize(), false );
        }
        else if(e.getButtonCode() == ButtonCode::cp_Subtract) 
        {
            updateCPButtons(false);

            m_Window->getSimulationDisplay()->updateMazeNode( m_Graph->removeTopCheckpoint(), m_Graph->getCellSize(), false );
        }

        return true;
    }

    bool Application::onPathfinderEvent(UpdatePathfinderEvent& e)
    {
        PathfinderType type;

        if(e.getButtonCode() == ButtonCode::pf_BFS) 
        {
            type = BFS;
        }
        else if(e.getButtonCode() == ButtonCode::pf_DFS) 
        {
            type = DFS;
        }

        SimulatePathfinding pf(m_Graph, m_Window, m_AnimationTimer);
        pf.run(true, type);

        return true;
    }
            
    bool Application::onGeneratorEvent(UpdateGeneratorEvent& e) 
    {
        GeneratorType type;

        if(e.getButtonCode() == ButtonCode::gen_Open) 
        {
            type = Open;
        }
        else if(e.getButtonCode() == ButtonCode::gen_DFS) 
        {
            type = DFSMaze;
        }

        SimulateGeneration gen(m_Graph, m_Window, m_AnimationTimer);
        gen.run(type);
        updateCPButtons(false);

        return true;
    }
    
    bool Application::onMousePressedEvent(MouseButtonPressedEvent& e)
    {
        if(isMouseInsideSimBounds( e.getX(), e.getY() ))
        {
            m_Graph->setMousePressed(e.getX(), e.getY());

            CellType type = m_Graph->getCellTypePressed();
            if(type == BlankCell || type == WallCell) 
            {
                handleMouseDrawingEvent();
            }
            else
            {
                handleMouseDraggingEvent();
            }
        }

        return true;
    }

    bool Application::onMouseReleasedEvent(MouseButtonReleasedEvent& e)
    {
        if(m_Graph->isMouseInteractive())
        {
            m_Graph->setMouseReleased();

            m_Graph->setDrawWallsMode(false);
            m_Graph->setEraseWallsMode(false);
        }

        return true;
    }

    bool Application::onMouseMovedEvent(MouseMovedEvent& e)
    {
        if(m_Graph->isMouseInteractive() && isMouseInsideSimBounds( e.getX(), e.getY() ) && m_Graph->isNewPosition( e.getX(), e.getY() ))
        {
            m_Graph->setMouseMoved( e.getX(), e.getY() );

            std::stack<MazeNode*> updatedNodes = m_Graph->getMouseUpdatedNodes(); // change to array
            while(updatedNodes.size() > 0)
            {
                m_Window->getSimulationDisplay()->updateMazeNode( updatedNodes.top(), m_Graph->getCellSize(), m_Graph->isMazeGenerated() );
                updatedNodes.pop();
            }

            if(m_IsInstantPathUpdatingEnabled && m_IsValidToInstantPathUpdate)   // this code ran if mouse moved on a non-BlankCell and after a SimulationPathfinding (or a "live simulation updating") just finished
            {
                SimulatePathfinding pf(m_Graph, m_Window, m_AnimationTimer);
                pf.run(false);
            }
        }

        return true;
    }
    
    bool Application::onSliderEvent(SliderMovedEvent& e)
    {
        int speedPercent = m_Window->getAnimationSpeedValue();
        m_Window->setSpeedText(speedPercent);

        //update animation timer
        m_AnimationTimer->updatePercentage(speedPercent);

        return true;
    }
    
    bool Application::onInstantPathUpdatingEvent(InstantPathUpdatingEvent& e)
    {
        m_IsInstantPathUpdatingEnabled = e.isChecked();

        return true;
    }
    
    bool Application::onWallCellClearEvent(WallCellClearEvent& e)
    {
        SimulatePathfinding pf(m_Graph, m_Window, m_AnimationTimer);
        pf.clearWalls();

        return true;
    }
    

    
    bool Application::isMouseInsideSimBounds(int x, int y)
    {
        // actual displayed size 
        int displayLengthUsed = (WALL_WIDTH + m_Graph->getCellSize()) * m_Graph->getMazeLength();

        bool isInsideXBounds = (DISPLAY_LEFT_BUFFER + WALL_WIDTH < x && x < DISPLAY_LEFT_BUFFER + displayLengthUsed);
        bool isInsideYBounds = (DISPLAY_TOP_BUFFER + WALL_WIDTH < y && y < DISPLAY_TOP_BUFFER + displayLengthUsed);

        return (isInsideXBounds && isInsideYBounds);
    }
    
    void Application::updateCPButtons(bool isIncrementingPositively)
    {        
        // enable if currently disabled
        if(!isIncrementingPositively && !m_Window->isAddCPEnabled())
        {
            m_Window->setAddCPEnabled(true);
        }
        else if(isIncrementingPositively && !m_Window->isRemoveCPEnabled())
        {
            m_Window->setRemoveCPEnabled(true);
        }

        // check to disable addCP
        if(m_Graph->getCheckpointCount() == CHECKPOINT_LIMIT - 1 && isIncrementingPositively)
        {
            m_Window->setAddCPEnabled(false);
        }
        // check to disable removeCP
        else if(m_Graph->getCheckpointCount() <= 1 && !isIncrementingPositively)
        {
            m_Window->setRemoveCPEnabled(false);
        }
    }

    void Application::handleMouseDrawingEvent()
    {
        if(!m_Graph->isMazeGenerated())
        {
            // setup
            m_IsValidToInstantPathUpdate = false;
            if(!m_Graph->isReadyForSimulation())
            {
                SimulatePathfinding pf(m_Graph, m_Window, m_AnimationTimer);
                pf.reset();
                m_Graph->setIsReadyForSimulation(true);
            }

            // execute drawEvent
            CellType type = m_Graph->getCellTypePressed();
            if(type == BlankCell) 
            {
                // enable draw walls 
                m_Graph->setDrawWallsMode(true);
            }
            else if(type == WallCell) 
            {
                // enable erase walls 
                m_Graph->setEraseWallsMode(true);
            }
            m_Window->getSimulationDisplay()->updateMazeNode( m_Graph->getMouseUpdatedNodes().top(), m_Graph->getCellSize(), m_Graph->isMazeGenerated() );
        }
        else
        {
            m_Graph->setMouseReleased();
            m_Graph->setDrawWallsMode(false);
            m_Graph->setEraseWallsMode(false);
        }
    }

    void Application::handleMouseDraggingEvent()
    {
        if(m_IsInstantPathUpdatingEnabled)// this code ran if wanting "live simulation updating"
        {
            m_IsValidToInstantPathUpdate = !m_Graph->isReadyForSimulation();
        }
        else if(!m_Graph->isReadyForSimulation())// this code ran if not wanting "live simulation updating"
        {
            SimulatePathfinding pf(m_Graph, m_Window, m_AnimationTimer);
            pf.reset();
            m_Graph->setIsReadyForSimulation(true);
        }
    }

}