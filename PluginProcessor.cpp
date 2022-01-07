/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
NewProjectAudioProcessor::NewProjectAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    ),
treeState(*this,nullptr,"PARAMETER_TREE",createParameterLayout())
#endif 
{
 
}

NewProjectAudioProcessor::~NewProjectAudioProcessor()
{
}

//==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout NewProjectAudioProcessor::createParameterLayout() // do i need to add 'fully-qualified-class? tutorial 44
{
    juce::AudioProcessorValueTreeState::ParameterLayout params;

    params.add(std::make_unique<juce::AudioParameterFloat>("Speed", "SPEED", 0.01f,1.0f,0.4f));

    params.add(std::make_unique<juce::AudioParameterBool>("Sync", "SYNC", false));
    params.add(std::make_unique<juce::AudioParameterBool>("Dot", "DOT", true));
    params.add(std::make_unique<juce::AudioParameterBool>("Trip", "TRIP", false));

    params.add(std::make_unique<juce::AudioParameterBool>("ForceStep", "STEP", false));

    for (int i = 1; i < 6; i++)
    {
        auto a = juce::String("OnButton"+ std::to_string(i));
     
        params.add(std::make_unique<juce::AudioParameterBool>(juce::String("bOnButton" + std::to_string(i)), juce::String("ON" + std::to_string(i)) ,false));
        params.add(std::make_unique<juce::AudioParameterBool>(juce::String("Reversed" + std::to_string(i)), juce::String("REVERSED" + std::to_string(i)), false));

        params.add(std::make_unique<juce::AudioParameterBool>(juce::String("PulseActive" + std::to_string(i)), juce::String("PULSEON" + std::to_string(i)), true));
        
        // THESE TWO **MUST** BE IN THIS ORDER WITHOUT INTERRUPTION (PluginEditor.cpp :: OrbitPanel :: addOrbit)
        params.add(std::make_unique<juce::AudioParameterInt>(juce::String("StepCount" + std::to_string(i)), juce::String("STEPS" + std::to_string(i)), 1, 32, 8));
        params.add(std::make_unique<juce::AudioParameterInt>(juce::String("PulseCount" + std::to_string(i)),juce::String( "PULSES" + std::to_string(i)), 0, 32, 3));

        params.add(std::make_unique<juce::AudioParameterChoice>(juce::String("OutputNote" + std::to_string(i)), juce::String("NOTE" + std::to_string(i)), juce::Array<juce::String>{ "C4", "C#4", "D4", "D#4", "E4", "F4", "F#4", "G4", "G#4", "A4", "A#4", "B4" },0));
        
        params.add(std::make_unique<juce::AudioParameterInt>(juce::String("iOctave" + std::to_string(i)), juce::String("OCTAVE" + std::to_string(i)), -3, 3, 0));
    }
        //params.push_back( std::make_unique<AudioParameterInt>(String(i), String(i), 0, i, 0) );
       
        // return { params.begin(), params.end() };
    return params;
}

const juce::String NewProjectAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NewProjectAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool NewProjectAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool NewProjectAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double NewProjectAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int NewProjectAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int NewProjectAudioProcessor::getCurrentProgram()
{
    return 0;
}

void NewProjectAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String NewProjectAudioProcessor::getProgramName(int index)
{
    return {};
}

void NewProjectAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
}

//==============================================================================
void NewProjectAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    time = 0;                               // [4]
    tempo = 112;
    currentStep.assign(5, 0);
    cycleChanged = true;
    rate = static_cast<float> (sampleRate); // [5]
}

void NewProjectAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NewProjectAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

juce::String vecToString(std::vector<bool> vec)
{
    juce::String s = "";
    for (auto v : vec)
    {
        s += (juce::String)((v)? "|true" : "|false");
    }
    return s;
}

void NewProjectAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{

    // the audio buffer in a midi effect will have zero channels!
    // but we need an audio buffer to getNumSamples....so....this next line will stay commented
    //jassert(buffer.getNumChannels() == 0);                                                         // [6]

    // however we use the buffer to get timing information
    auto numSamples = buffer.getNumSamples();                                                       // [7]

    juce::MidiBuffer processedMidi;

    //bool done = false;

    if (getPlayHead() != nullptr)
        getPlayHead()->getCurrentPosition(playHeadInfo);
    tempo = playHeadInfo.bpm;
    numerator = playHeadInfo.timeSigNumerator;

    auto speed = treeState.getRawParameterValue("Speed");
    auto sync = treeState.getRawParameterValue("Sync");
    auto dot = treeState.getRawParameterValue("Dot");
    auto trip = treeState.getRawParameterValue("Trip");

    // get note duration
    syncSpeed = 1 / std::pow(2.0f, (*speed * 100.0f) - 90.0f); // the editor changes range from 90-100 with sync on. this function gives me denomenator of note value
    auto noteDuration = (!*sync) ?
        static_cast<int> (std::ceil(rate * 0.25f * (0.1f + (1.0f - (*speed)))))
        : static_cast<int> (std::ceil(rate * 0.25f * (tempo / 60) * numerator * syncSpeed)); // should correspond to one quarter note w/o syncSpeed
   
    if (*dot)
        noteDuration = noteDuration * 1.5f;
    if (*trip)
        noteDuration = (noteDuration * 2.0f) / 3.0f;

    //noteDuration *= 10;

    if (ntDrtn != noteDuration)
        ntDrtn = noteDuration;

    done = true;
    //I only want to do this loop if a value has changed....
    if (cycleChanged)
    {
        //done = false;
        cycleChanged = false;
        orbits.clear();
        orbits.assign(5, { false });
        
        for (int i = 0; i < 5; i++)
        {

            steps = (int)(*treeState.getRawParameterValue("StepCount" + std::to_string(i + 1)));
            pulses = (int)(*treeState.getRawParameterValue("PulseCount" + std::to_string(i + 1)));
         
            orbits[i].assign(steps, false);
            auto check = std::string(orbits[i].begin(), orbits[i].end());


            if (pulses >= steps)
            {
                orbits[i].assign(steps, true);
            }

            if (pulses > 0 && pulses < steps)
            {
                //evenly distributes steps of the orbit
                for (int x = 0; x < pulses; x++)
                {
                    cycleSteps.push_back((int)(steps / pulses));
                }


                //evenly spaces the remainder across the orbit 
                auto m = steps % pulses;
                if (m)
                {
                    auto skip = (int)(juce::roundToIntAccurate(pulses / m));

                    for (int x = 0; x < pulses; x += (int)(juce::roundToIntAccurate(pulses / m)))
                    {
                        cycleSteps[x] = cycleSteps[x] + 1;

                        if (m - (x + 1) == 0) // break loop when we've added 'remainder' number of steps between pulses
                        {
                            x = pulses;
                        }
                    }
                }

                auto checkCycleSteps = std::string(cycleSteps.begin(), cycleSteps.end());


                // adds pulse pattern to the vector of orbits
                auto pulseLocation = (uint8)0;
                for (auto x : cycleSteps)
                {
                    pulseLocation += x;
                    orbits[i][pulseLocation % steps] = true;
                }

                for (auto o : orbits)
                {
                    auto checkOrbits = vecToString(o);
                }
            }


            cycleSteps.clear();
        }
        //done = true;
    }
    


    // .........................................................................................................................


    if ((time + numSamples) >= noteDuration && done)                                                      
    {  
        auto offset = juce::jmax(0, juce::jmin((int)(noteDuration - time), numSamples - 1));

        for(auto note : notes)
            processedMidi.addEvent(juce::MidiMessage::noteOff(1, note), offset);
        notes.clear();

        // every cycle moves a step 
        for (int i = 0; i < 5; i++)
        {
            steps = (int)(*treeState.getRawParameterValue("StepCount" + std::to_string(i + 1)));

            currentStep[i] = ((int)(*treeState.getRawParameterValue("Reversed" + std::to_string(i + 1))) == false) ?
                (currentStep[i]+1) % steps 
                : steps - ( (steps-currentStep[i]) % steps ) - 1;

            //add note for each orbit with currentStep that returns 'true'   
            for (auto o : orbits)
            {
                auto checkOrbits = std::string(o.begin(), o.end());
            }


            if ( orbits[i][currentStep[i]] )
            {
                treeState.getParameter("PulseActive" + std::to_string(i + 1))->setValueNotifyingHost(1.0f);
                
               
                auto note = noteToInt(treeState.getParameter("OutputNote" + std::to_string(i + 1))->getCurrentValueAsText());
                note = note + ( 12 * (int)(*treeState.getRawParameterValue("iOctave" + std::to_string(i + 1))) );
                processedMidi.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8)84), offset);
                notes.add(note);
            }
            else
            {
                treeState.getParameter("PulseActive" + std::to_string(i + 1))->setValueNotifyingHost(0.0f);
            } 

        }

        treeState.getParameter("ForceStep")->setValueNotifyingHost(!treeState.getRawParameterValue("ForceStep"));// should ping OrbitComponent to repaint
    }

    time = (time + numSamples) % noteDuration;                                                      

    //always use swapWith(), avoids unpredictable behavior from directly editing midi buffer
   
    midi.swapWith(processedMidi);
}

//==============================================================================
bool NewProjectAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* NewProjectAudioProcessor::createEditor()
{
    return new AarrowAudioProcessorEditor(*this);
}

//==============================================================================
void NewProjectAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.

    auto state = treeState.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
   
}

void NewProjectAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.

    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(treeState.state.getType()))
            treeState.replaceState(juce::ValueTree::fromXml(*xmlState));
    

}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NewProjectAudioProcessor();
}
