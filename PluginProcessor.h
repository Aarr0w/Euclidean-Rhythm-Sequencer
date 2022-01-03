/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
class NewProjectAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    NewProjectAudioProcessor();
    ~NewProjectAudioProcessor() override;

    juce::AudioProcessorValueTreeState treeState;
    std::vector<int> currentStep;
    
    //==============================================================================
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif
    int noteToInt(juce::String s)
    {
        int noteValue = 0;
        char c = s[0];
       
        switch (c)
        {
           
            case 'C': noteValue = 60;
            case 'D': noteValue = 62;
            case 'E': noteValue = 64;
            case 'F': noteValue = 65;
            case 'G': noteValue = 67;
            case 'A': noteValue = 69;
            case 'B': noteValue = 71;
        }

        if(s.contains("#"))
        {
            noteValue += 1;
        }

        return noteValue;
    }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

private:
    //==============================================================================

    //juce::AudioProcessorValueTreeState treeState;

    juce::AudioPlayHead::CurrentPositionInfo playHeadInfo;

    int tempo, time, numerator;
    int ntDrtn;
    float rate;
    float syncSpeed;
    int steps;
    int pulses;
    bool cycleChanged;
    std::vector <std::vector<bool>> orbits;
    std::vector<int> cycleSteps;
    juce::SortedSet<int> notes; //might need to be vector if noteOffs aren't catching multiples

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NewProjectAudioProcessor)
};