#include "Common.h"

// Check for Work Graphs 1.1 (Mesh Nodes) support
#if DEVICE_SUPPORTED_WORK_GRAPHS_TIER < 11
#error "The current device does not support Work Graphs tier 1.1 (Mesh Nodes), which is required for this sample."
#endif

#include "Skybox.h"
#include "SplineSegment.h"
#include "Leaves.h"
#include "Fruits.h"
#include "TreeGeneration.h"

[Shader("node")]
[NodeIsProgramEntry]
[NodeLaunch("thread")]
[NodeId("Entry", 0)]
void EntryFunction(
    [MaxRecords(1)]
    [NodeId("Stem")]
    NodeOutput<GenerateTreeRecord> output,

    [MaxRecords(1)]
    [NodeId("Skybox")]
    NodeOutput<SkyboxRecord> skyboxOutput,

    [MaxRecords(1)]
    [NodeId("UserInterface")]
    EmptyNodeOutput userInterfaceOutput
)
{
    // Check and init persistent config with default values
    if (PersistentScratchBuffer.Load<uint>(0) == 0) {
        // Store default values
        StorePersistentConfig(PersistentConfig::TREE_TYPE, 0);
        StorePersistentConfig(PersistentConfig::TREE_ATTRACTION_UP, GetTreeParameters().AttractionUp);
        StorePersistentConfig(PersistentConfig::SEASON, 2.f);
        StorePersistentConfig(PersistentConfig::WIND_STRENGTH, 5.f);


        const TreeParameters params = GetTreeParameters();
        const float treeHeight = params.Scale * params.nLength[0];

        StorePersistentConfig(PersistentConfig::CAMERA_YAW, 0.f);
        StorePersistentConfig(PersistentConfig::CAMERA_PITCH, 20.f);
        StorePersistentConfig(PersistentConfig::CAMERA_DISTANCE, treeHeight * 1.2f);

        StorePersistentConfig(PersistentConfig::TIME, Time);
        StorePersistentConfig(PersistentConfig::MOUSE_X, MousePosition.x);
        StorePersistentConfig(PersistentConfig::MOUSE_Y, MousePosition.y);
        StorePersistentConfig(PersistentConfig::KEY_SPACE_DOWN, 0);

        // Mark config initialized
        PersistentScratchBuffer.Store<uint>(0, 1);
    } else {
        // Handle user input
        if (input::IsMouseRightDown()) {
            const float previousMousePositionX = LoadPersistentConfigFloat(PersistentConfig::MOUSE_X);
            const float previousMousePositionY = LoadPersistentConfigFloat(PersistentConfig::MOUSE_Y);

            const float2 mouseDelta = MousePosition - float2(previousMousePositionX, previousMousePositionY);

            const float yaw   = LoadPersistentConfigFloat(PersistentConfig::CAMERA_YAW) + mouseDelta.x * 0.5f;
            const float pitch = LoadPersistentConfigFloat(PersistentConfig::CAMERA_PITCH) + mouseDelta.y * 0.5f;

            StorePersistentConfig(PersistentConfig::CAMERA_YAW, yaw);
            StorePersistentConfig(PersistentConfig::CAMERA_PITCH, clamp(pitch, -20.f, 80.f));
        }

        float distanceDelta = 0;
        if (input::IsKeySDown() || input::IsKeyDownArrowDown()) {
            distanceDelta += 1.f;
        }
        if (input::IsKeyWDown() || input::IsKeyUpArrowDown()) {
            distanceDelta -= 1.f;
        }

        const float timeDelta = Time - LoadPersistentConfigFloat(PersistentConfig::TIME);
        const float distance  = LoadPersistentConfigFloat(PersistentConfig::CAMERA_DISTANCE) + distanceDelta * timeDelta * 50.f;
        StorePersistentConfig(PersistentConfig::CAMERA_DISTANCE, clamp(distance, 2.f, 80.f));

        const bool keySpaceDown    = input::IsKeySpaceDown();
        const bool keySpaceWasDown = LoadPersistentConfigUint(PersistentConfig::KEY_SPACE_DOWN);
        const bool keySpacePressed = keySpaceDown && !keySpaceWasDown;

        if (keySpacePressed) {
            const uint treeType = LoadPersistentConfigUint(PersistentConfig::TREE_TYPE);
            StorePersistentConfig(PersistentConfig::TREE_TYPE, (treeType + 1) % TREE_TYPE_COUNT);

            // Reset tree parameters
            StorePersistentConfig(PersistentConfig::TREE_ATTRACTION_UP, GetTreeParameters().AttractionUp);
        }

        // store time for next frame
        StorePersistentConfig(PersistentConfig::TIME, Time);
        // store key state for next frame
        StorePersistentConfig(PersistentConfig::KEY_SPACE_DOWN, uint(keySpaceDown));
        // Store mouse position to compute delta in next frame
        StorePersistentConfig(PersistentConfig::MOUSE_X, MousePosition.x);
        StorePersistentConfig(PersistentConfig::MOUSE_Y, MousePosition.y);
    }

    // kick off rendering UI
    userInterfaceOutput.ThreadIncrementOutputCount(1);

    // draw skybox
    ThreadNodeOutputRecords<SkyboxRecord> skyboxRecord = skyboxOutput.GetThreadNodeOutputRecords(1);
    skyboxRecord.Get().test = 0;
    skyboxRecord.OutputComplete();

    // draw tree
    ThreadNodeOutputRecords<GenerateTreeRecord> outputRecord = output.GetThreadNodeOutputRecords(1);
    // create tree record at origin with seed
    outputRecord.Get() = CreateTreeRecord(float3(0, 0, 0), qRotateX(PI*-.5), LoadPersistentConfigUint(PersistentConfig::SEED));
    outputRecord.OutputComplete();
}

// ============================ UI ====================

void Slider(inout Cursor cursor, in PersistentConfig config, in float valueMin, in float valueMax, bool integer = false)
{
    const int2 position     = cursor.position;
    const int  width        = 200;
    const int  height       = 30;
    const int  innerPadding = 3;
    const int  handleWidth  = 10;
    const int  trackWidth   = width - (2 * innerPadding) - handleWidth;

    const int2 topLeft     = position;
    const int2 bottomRight = position + int2(width, height);

    FillRect(topLeft, bottomRight, float3(0.5, 0.5, 0.5));
    DrawRect(topLeft, bottomRight, 1);

    float value        = integer? LoadPersistentConfigUint(config) : LoadPersistentConfigFloat(config);
    float valuePercent = MapRange(value, valueMin, valueMax, 0.f, 1.f);

    int2 handleTopLeft     = position + innerPadding + int2(valuePercent * trackWidth, 0);
    int2 handleBottomRight = handleTopLeft + int2(handleWidth, height - 2 * innerPadding);

    const bool mouseOverHandle = all(MousePosition >= handleTopLeft) && all(MousePosition <= handleBottomRight);
    const bool mouseOverSlider = all(MousePosition >= topLeft) && all(MousePosition <= bottomRight);
    const bool sliderActive    = mouseOverSlider && input::IsMouseLeftDown();

    if (sliderActive) {
        const int x  = MousePosition.x - (handleWidth / 2) - (position.x + innerPadding);
        valuePercent = clamp(x / float(trackWidth), 0.f, 1.f);
        value        = MapRange(valuePercent, 0.f, 1.f, valueMin, valueMax);

        if (integer) {
            StorePersistentConfig(config, uint(value));
        } else {
            StorePersistentConfig(config, value);
        }

        handleTopLeft     = position + innerPadding + int2(valuePercent * trackWidth, 0);
        handleBottomRight = handleTopLeft + int2(handleWidth, height - (2 * innerPadding));
    }

    const float3 handleColor = mouseOverHandle? (sliderActive? float3(0.3, 0.3, 0.6) : float3(0.3, 0.3, 0.3)) : float3(0, 0, 0);
    FillRect(handleTopLeft, handleBottomRight, handleColor);

    Cursor valueCursor = Cursor(position + int2(10, 8), 2, float3(1, 1, 1));
    if (integer) {
        PrintUint(valueCursor, uint(value));
    } else {
        PrintFloat(valueCursor, value);
    }

    cursor.position.y += height;
    cursor.Newline();
}

Cursor GetUserInterfaceCursor(uint l) {
    return Cursor(float2(10, 30 + l * 16), 2, float3(0, 0, 0));
}

[Shader("node")]
[NodeLaunch("thread")]
[NodeId("UserInterface")]
void UserInterface()
{
    // Dummy node, UI is rendered in other nodes below
    // This was done to speed up compilation times significantly in some cases
    // While this is a cute example of NodeShareInputOf() we do not recommend to print (or program your GPU) this way
}

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NodeShareInputOf("UserInterface")]
[NumThreads(40, 1, 1)]
void UserInterface_Line_0(uint gtid : SV_GROUPTHREADID)
{
    Cursor cursor = GetUserInterfaceCursor(0);
    cursor.Right(gtid);

    printutil::PrintChar(cursor, printutil::CharToInt("Hold Right Mouse Button to Rotate Camera"[gtid]));
}

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NodeShareInputOf("UserInterface")]
[NumThreads(25, 1, 1)]
void UserInterface_Line_1(uint gtid : SV_GROUPTHREADID)
{
    Cursor cursor = GetUserInterfaceCursor(1);
    cursor.Down(1);
    cursor.Right(gtid);

    printutil::PrintChar(cursor, printutil::CharToInt("Space to Change Tree Type"[gtid]));
}

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NodeShareInputOf("UserInterface")]
[NumThreads(35, 1, 1)]
void UserInterface_Line_2(uint gtid : SV_GROUPTHREADID)
{
    Cursor cursor = GetUserInterfaceCursor(2);
    cursor.Down(2);
    cursor.Right(gtid);

    printutil::PrintChar(cursor, printutil::CharToInt("W/S or Up/Down Arrow to Zoom In/Out"[gtid]));
}

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NodeShareInputOf("UserInterface")]
[NumThreads(32, 1, 1)]
void UserInterface_Tree_Type(uint gtid : SV_GROUPTHREADID)
{
    Cursor cursor = GetUserInterfaceCursor(4);
    cursor.Down(6);
    cursor.Right(gtid);
    
    if(gtid < 10){
        printutil::PrintChar(cursor, printutil::CharToInt("Tree Type:"[gtid]));
    }   
    
    cursor.Left(gtid);
    cursor.Right(10);
    cursor.Right(gtid);
    
    const uint treeType = LoadPersistentConfigUint(PersistentConfig::TREE_TYPE);
    
    if (treeType == TREE_TYPE_APPLE && gtid < 5) {
         printutil::PrintChar(cursor, printutil::CharToInt("Apple"[gtid]));
    } else if (treeType == TREE_TYPE_SASSAFRAS && gtid < 9) {
        printutil::PrintChar(cursor, printutil::CharToInt("Sassafras"[gtid]));
    } else if (treeType == TREE_TYPE_PALM && gtid < 4) {
        printutil::PrintChar(cursor, printutil::CharToInt("Palm"[gtid]));
    } else if (treeType == TREE_TYPE_TAMARACK && gtid < 8) {
        printutil::PrintChar(cursor, printutil::CharToInt("Tamarack"[gtid]));
    }
}

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NodeShareInputOf("UserInterface")]
[NumThreads(13, 1, 1)]
void UserInterface_Slider_0(uint gtid : SV_GROUPTHREADID)
{
    Cursor cursor = GetUserInterfaceCursor(6);
    cursor.Down(9);
    cursor.Right(gtid);

    printutil::PrintChar(cursor, printutil::CharToInt("Attraction Up"[gtid]));
    
    if (gtid == 0) {
        cursor.Newline();
        Slider(cursor, PersistentConfig::TREE_ATTRACTION_UP, -2, 2);
    }
}

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NodeShareInputOf("UserInterface")]
[NumThreads(6, 1, 1)]
void UserInterface_Slider_1(uint gtid : SV_GROUPTHREADID)
{
    Cursor cursor = GetUserInterfaceCursor(7);
    cursor.Down(13);
    cursor.Right(gtid);

    printutil::PrintChar(cursor, printutil::CharToInt("Season"[gtid]));
    
    if (gtid == 0) {
        cursor.Newline();
        Slider(cursor, PersistentConfig::SEASON, 0, 4);
    }
}

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NodeShareInputOf("UserInterface")]
[NumThreads(13, 1, 1)]
void UserInterface_Slider_2(uint gtid : SV_GROUPTHREADID)
{
    Cursor cursor = GetUserInterfaceCursor(8);
    cursor.Down(17);
    cursor.Right(gtid);

    printutil::PrintChar(cursor, printutil::CharToInt("Wind Strength"[gtid]));
    
    if (gtid == 0) {
        cursor.Newline();
        Slider(cursor, PersistentConfig::WIND_STRENGTH, 0, 20);
    }
}

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NodeShareInputOf("UserInterface")]
[NumThreads(4, 1, 1)]
void UserInterface_Slider_3(uint gtid : SV_GROUPTHREADID)
{
    Cursor cursor = GetUserInterfaceCursor(9);
    cursor.Down(21);
    cursor.Right(gtid);

    printutil::PrintChar(cursor, printutil::CharToInt("Seed"[gtid]));
    
    if (gtid == 0) {
        cursor.Newline();
        Slider(cursor, PersistentConfig::SEED, 0, 200, true);
    }
}
