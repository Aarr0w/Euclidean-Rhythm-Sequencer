/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"



//=======================================================================================
class ParameterListener : private juce::AudioProcessorParameter::Listener,
    private juce::AudioProcessorListener,
    private juce::Timer
{
public:
    ParameterListener(juce::AudioProcessor& proc, juce::AudioProcessorParameter& param)
        : processor(proc), parameter(param)
    {

        parameter.addListener(this);

        startTimer(100);
    }

    ~ParameterListener() override
    {
        parameter.removeListener(this);
    }

    juce::AudioProcessorParameter& getParameter() const noexcept
    {
        return parameter;
    }

    virtual void handleNewParameterValue() = 0;

private:
    //==============================================================================
    void parameterValueChanged(int, float) override
    {
        parameterValueHasChanged = 1;
    }

    void parameterGestureChanged(int, bool) override {}

    //==============================================================================
    void audioProcessorParameterChanged(juce::AudioProcessor*, int index, float) override
    {
        if (index == parameter.getParameterIndex())
            parameterValueHasChanged = 1;
    }

    void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails&) override {}

    //==============================================================================
    void timerCallback() override
    {
        if (parameterValueHasChanged.compareAndSetBool(0, 1))
        {
            handleNewParameterValue();
            startTimerHz(50);
        }
        else
        {
            startTimer(juce::jmin(250, getTimerInterval() + 10));
        }
    }

    juce::AudioProcessor& processor;
    juce::AudioProcessorParameter& parameter;
    juce::Atomic<int> parameterValueHasChanged{ 0 };


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterListener)
};

//============================================================================================================
class SliderParameterComponent final : public juce::Component,
    private ParameterListener
{
public:
    SliderParameterComponent(juce::AudioProcessor& proc, juce::AudioProcessorParameter& param)
        : ParameterListener(proc, param)
    {
        //link = NULL;

        if (getParameter().getNumSteps() != juce::AudioProcessor::getDefaultNumParameterSteps())
            slider.setRange(0.0, 1.0, 1.0 / (getParameter().getNumSteps() - 1.0));
        else
            slider.setRange(0.0, 1.0);


        slider.setRange(0.0, 1.0);
        //slider.setValue((int)getParameter().getDefaultValue());
        getParameter().setValue(getParameter().getDefaultValue());
        slider.setScrollWheelEnabled(false);
        addAndMakeVisible(slider);

        //valueLabel.setColour(juce::Label::outlineColourId, slider.findColour(juce::Slider::textBoxOutlineColourId));
        valueLabel.setColour(juce::Label::outlineColourId, juce::Colours::transparentWhite);
        valueLabel.setColour(juce::Label::textColourId, juce::Colours::cyan);
        valueLabel.setBorderSize({ 1, 1, 1, 1 });
        valueLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(valueLabel);

        // Set the initial value.
        handleNewParameterValue();

        slider.onValueChange = [this] { sliderValueChanged(); };
        slider.onDragStart = [this] { sliderStartedDragging(); };
        slider.onDragEnd = [this] { sliderStoppedDragging(); };
    }

    void paint(juce::Graphics&) override {}

    void resized() override
    {
        auto area = getLocalBounds().reduced(0, 10);

        if (slider.isHorizontal())
            valueLabel.setBounds(area.removeFromRight(getWidth() / 4));
        else
            valueLabel.setBounds(area.removeFromTop(10));

        area.removeFromLeft(6);
        slider.setBounds(area);
    }

    void changeSliderStyle(int style)
    {
        switch (style)
        {
            case 0: slider.setSliderStyle(juce::Slider::Rotary);            break;
            case 1: slider.setSliderStyle(juce::Slider::LinearVertical);    break;
            case 2: slider.setSliderStyle(juce::Slider::LinearBarVertical); break;
            case 3: slider.setSliderStyle(juce::Slider::LinearBar);         break;
            default: slider.setSliderStyle(juce::Slider::LinearHorizontal); break;
        }
    }

    void setLink(juce::Component& l)
    {
        link = &l;
    }

    void linkAction(bool on)
    {
        if (!on)
        {
            slider.setColour(juce::Slider::trackColourId, juce::Colours::slategrey.withBrightness(0.5f));
            slider.setColour(juce::Slider::thumbColourId, juce::Colours::grey);
            valueLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
        }
        else
        {
            slider.setColour(juce::Slider::trackColourId, getLookAndFeel().findColour(juce::Slider::trackColourId));
            slider.setColour(juce::Slider::thumbColourId, getLookAndFeel().findColour(juce::Slider::thumbColourId));
            valueLabel.setColour(juce::Label::textColourId, getLookAndFeel().findColour(juce::Label::textColourId));
        }
    }

    void setSliderSkew(float f)
    {
        slider.setSkewFactor(f);
    }

    void setSliderTooltip(juce::String s)
    {
        slider.setTooltip(s);
    }


private:
    void updateTextDisplay()
    {
        valueLabel.setText(getParameter().getCurrentValueAsText(), juce::dontSendNotification);
    }

    void handleNewParameterValue() override
    {
        if (!isDragging)
        {
            slider.setValue(getParameter().getValue(), juce::dontSendNotification);
            updateTextDisplay();
        }
    }

    void sliderValueChanged()
    {
        auto newVal = (float)slider.getValue();

        if (getParameter().getValue() != newVal)
        {
            if (!isDragging)
                getParameter().beginChangeGesture();

            getParameter().setValueNotifyingHost((float)slider.getValue());
            updateTextDisplay();

            if (!isDragging)
                getParameter().endChangeGesture();
        }
    }

    void sliderStartedDragging()
    {
        isDragging = true;
        getParameter().beginChangeGesture();
    }

    void sliderStoppedDragging()
    {
        isDragging = false;
        getParameter().endChangeGesture();
    }

    juce::Slider slider{ juce::Slider::LinearHorizontal, juce::Slider::TextEntryBoxPosition::NoTextBox };
    juce::Component* link;
    juce::Label valueLabel;
    bool isDragging = false;
    bool bpm = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SliderParameterComponent)
};

//================================================================================================================

class BooleanButtonParameterComponent final : public juce::Component,
    private ParameterListener
{
public:
    BooleanButtonParameterComponent(juce::AudioProcessor& proc, juce::AudioProcessorParameter& param, juce::String buttonName)
        : ParameterListener(proc, param)
    {
        link = nullptr;
        // Set the initial value.
        button.setButtonText(buttonName);
        getParameter().setValue(getParameter().getDefaultValue());
        handleNewParameterValue();
        button.onClick = [this] { buttonClicked(); };
        button.setClickingTogglesState(true);
        addAndMakeVisible(button);
    }

    void paint(juce::Graphics&) override {}

    void resized() override
    {
        auto area = getLocalBounds();
        area.removeFromLeft(8);
        button.setBounds(area.reduced(0, 8)); // (0,10)
    }

    void setLink(juce::Component& l)
    {
        link = &l;
    }

    void linkAction()
    {
    }

private:
    void handleNewParameterValue() override
    {
        button.setToggleState(isParameterOn(), juce::dontSendNotification);
    }


    void buttonClicked()
    {
        if (isParameterOn() != button.getToggleState())
        {
            getParameter().beginChangeGesture();
            getParameter().setValueNotifyingHost(button.getToggleState() ? 1.0f : 0.0f);
            getParameter().endChangeGesture();
            if (link)
                dynamic_cast<SliderParameterComponent*>(link)->linkAction(button.getToggleState());


        }
    }

    bool isParameterOn() const { return getParameter().getValue() >= 0.5f; }
    juce::Component* link;
    juce::TextButton button;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BooleanButtonParameterComponent)
};
//==============================================================================
class BooleanParameterComponent final : public juce::Component,
    private ParameterListener
{
public:
    BooleanParameterComponent(juce::AudioProcessor& proc, juce::AudioProcessorParameter& param, juce::String buttonName)
        : ParameterListener(proc, param)
    {

        // Set the initial value.
        button.setButtonText(buttonName.substring(1));
        getParameter().setValue(getParameter().getDefaultValue());
        handleNewParameterValue();
        button.onClick = [this] { buttonClicked(); };
        addAndMakeVisible(button);
    }

    void paint(juce::Graphics&) override {}

    void resized() override
    {
        auto area = getLocalBounds();
        //area.removeFromLeft(8);
        button.setBounds(area.reduced(0, 10));  //(0,10)
    }

    void setLink(Component& l)
    {
        link = &l;
    }

    void linkAction()
    {
    }

private:
    void handleNewParameterValue() override
    {
        button.setToggleState(isParameterOn(), juce::dontSendNotification);
    }

    void buttonClicked()
    {
        if (isParameterOn() != button.getToggleState())
        {
            getParameter().beginChangeGesture();
            getParameter().setValueNotifyingHost(button.getToggleState() ? 1.0f : 0.0f);
            getParameter().endChangeGesture();
        }

    }

    bool isParameterOn() const { return getParameter().getValue() >= 0.5f; }

    juce::Component* link;
    juce::ToggleButton button;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BooleanParameterComponent)
};
//==============================================================================
class SwitchButtonParameterComponent final : public juce::Component,    // single button, swaps text on click
    private ParameterListener
{
public:
    SwitchButtonParameterComponent(juce::AudioProcessor& proc, juce::AudioProcessorParameter& param)
        : ParameterListener(proc, param)
    {
        link = nullptr;
        getParameter().setValue(getParameter().getDefaultValue());
        index = (int)(getParameter().getValue());
        button.setButtonText(getParameter().getCurrentValueAsText());
        handleNewParameterValue();
        button.onClick = [this] { buttonClicked(); };
        button.setClickingTogglesState(true);
        addAndMakeVisible(button);
        button.setColour(juce::TextButton::textColourOffId, juce::Colours::grey);
        //////////////////////////////////////////////////////////////////////////////////

        // Set the initial value.

        handleNewParameterValue();


        addAndMakeVisible(button);
    }

    void paint(juce::Graphics&) override {}

    void resized() override
    {
        auto area = getLocalBounds(); // (0,8)
        area.removeFromLeft(8);
        button.setBounds(area.reduced(0, 8)); // (0,10)
    }

    void setLink(Component& l)
    {
        link = &l;
    }

    void linkAction()
    {
    }

private:
    void handleNewParameterValue() override
    {
        button.setButtonText(getParameter().getCurrentValueAsText());
    }


    void buttonClicked()
    {
        index = (index + 1) % getParameter().getAllValueStrings().size();
        auto selectedText = getParameter().getAllValueStrings()[index];
        getParameter().setValueNotifyingHost(getParameter().getValueForText(selectedText));
        button.setButtonText(getParameter().getCurrentValueAsText());
        if (link)
            dynamic_cast <SliderParameterComponent*> (link)->linkAction(button.getToggleState());
    }


    int index;
    Component* link;
    juce::TextButton button;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SwitchButtonParameterComponent)
};

//==============================================================================
class SwitchParameterComponent final : public juce::Component,
    private ParameterListener
{
public:
    SwitchParameterComponent(juce::AudioProcessor& proc, juce::AudioProcessorParameter& param)
        : ParameterListener(proc, param)
    {
        link = NULL;

        n = getParameter().getNumSteps();
        float x = 1.0 / (n - 1);
        for (float i = 0.0; i < n; i += 1.0)
        {   // not really sure how the 'normalized value' iterator works but this does the trick for now
            auto* b = buttons.add(new juce::TextButton(getParameter().getText((0.0f + (x * i)), 16)));

            addAndMakeVisible(b);
            b->setRadioGroupId(42);
            b->setClickingTogglesState(true);

            b->setToggleState(false, juce::dontSendNotification);
            b->onClick = [this, i] { aButtonChanged(i); };

            if (i == 0)
                b->setConnectedEdges(juce::Button::ConnectedOnRight);
            else
                if (i == (n - 1))
                    b->setConnectedEdges(juce::Button::ConnectedOnLeft);
                else
                    b->setConnectedEdges(juce::Button::ConnectedOnRight + juce::Button::ConnectedOnLeft);

        }
        //////////////////////////////////////////////////////////////////////////////////

        // Set the initial value.
        buttons[0]->setToggleState(true, juce::dontSendNotification);
        aButtonChanged(0);
        //handleNewParameterValue();


        for (auto& button : buttons)
            addAndMakeVisible(button);
    }

    void paint(juce::Graphics&) override {}

    void resized() override
    {
        auto area = getLocalBounds().reduced(0, 8); // (0,8)
        //area.removeFromLeft(8);

        for (int i = 0; i < n; ++i)
        {
            buttons[i]->setBounds(area.removeFromLeft((getWidth() / n))); //60
        }

    }

    void setLink(Component& l)
    {
        link = &l;
    }

    void linkAction()
    {
    }

private:
    void handleNewParameterValue() override
    {
        int newState = getCurrentState();

        for (int i = 0; i < buttons.size(); i++)
        {
            if (i == newState && (buttons[i]->getToggleState() == false))
            {
                buttons[i]->setToggleState(true, juce::dontSendNotification);
            }

        }


    }


    void aButtonChanged(int i)
    {
        auto buttonState = (*buttons[i]).getToggleState();

        if (getCurrentState() != i)
        {
            getParameter().beginChangeGesture();

            if (getParameter().getAllValueStrings().isEmpty())
            {
                getParameter().setValueNotifyingHost(float(i));
            }
            else
            {
                // When a parameter provides a list of strings we must set its
                // value using those strings, rather than a float, because VSTs can
                // have uneven spacing between the different allowed values and we
                // want the snapping behaviour to be consistent with what we do with
                // a combo box.
                auto selectedText = (*buttons[i]).getButtonText();
                getParameter().setValueNotifyingHost(getParameter().getValueForText(selectedText));
            }

            getParameter().endChangeGesture();
        }
    }

    int getCurrentState()
    {
        return (int)(getParameter().getAllValueStrings()
            .indexOf(getParameter().getCurrentValueAsText()));
    }

    bool isParameterOn() const
    {
        if (getParameter().getAllValueStrings().isEmpty())
            return getParameter().getValue() > 0.5f;

        auto index = getParameter().getAllValueStrings()
            .indexOf(getParameter().getCurrentValueAsText());

        if (index < 0)
        {
            // The parameter is producing some unexpected text, so we'll do
            // some linear interpolation.
            index = juce::roundToInt(getParameter().getValue());
        }

        return index == 1;
    }

    //juce::TextButton buttons[3];
    float n;
    Component* link;
    juce::OwnedArray<juce::TextButton> buttons;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SwitchParameterComponent)
};
//==============================================================================
class IncrementParameterComponent final : public juce::Component,
    private ParameterListener
{
public:
    IncrementParameterComponent(juce::AudioProcessor& proc, juce::AudioProcessorParameter& param)
        : ParameterListener(proc, param)
    {
        link = NULL;

        if (getParameter().getNumSteps() != juce::AudioProcessor::getDefaultNumParameterSteps())
            box.setRange(0.0, 1.0, 1.0 / (getParameter().getNumSteps() - 1.0));
        else
            box.setRange(0.0, 1.0);

        getParameter().setValue(getParameter().getDefaultValue());

        box.setScrollWheelEnabled(false);
        addAndMakeVisible(box);

        valueLabel.setColour(juce::Label::outlineColourId, box.findColour(juce::Slider::textBoxOutlineColourId));
        valueLabel.setColour(juce::Label::textColourId, juce::Colours::cyan);
        valueLabel.setBorderSize({ 1, 1, 1, 1 });
        valueLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(valueLabel);

        // Set the initial value.
        handleNewParameterValue();

        box.onValueChange = [this] { sliderValueChanged(); };
        box.onDragStart = [this] { sliderStartedDragging(); };
        box.onDragEnd = [this] { sliderStoppedDragging(); };
    }

    void paint(juce::Graphics&) override {}

    void resized() override
    {
        auto area = getLocalBounds().reduced(0, 10);

        valueLabel.setBounds(area.removeFromRight(80)); //80

        area.removeFromLeft(20);
        area.removeFromRight(20);
        box.setBounds(area);
    }

    void setLink(juce::Component& l)
    {
        link = &l;
    }

    void linkAction()
    {
    }

private:
    void updateTextDisplay()
    {
        valueLabel.setText(getParameter().getCurrentValueAsText(), juce::dontSendNotification);
    }

    void handleNewParameterValue() override
    {
        if (!isDragging)
        {
            box.setValue(getParameter().getValue(), juce::dontSendNotification);
            updateTextDisplay();
        }
    }

    void sliderValueChanged()
    {
        auto newVal = (float)box.getValue();

        if (getParameter().getValue() != newVal)
        {
            if (!isDragging)
                getParameter().beginChangeGesture();

            getParameter().setValueNotifyingHost((float)box.getValue());
            updateTextDisplay();

            if (!isDragging)
                getParameter().endChangeGesture();
        }
    }

    void sliderStartedDragging()
    {
        isDragging = true;
        getParameter().beginChangeGesture();
    }

    void sliderStoppedDragging()
    {
        isDragging = false;
        getParameter().endChangeGesture();
    }

    juce::Component* link;
    juce::Slider box{ juce::Slider::IncDecButtons, juce::Slider::TextEntryBoxPosition::NoTextBox };
    juce::Label valueLabel;
    bool isDragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IncrementParameterComponent)
};
//
//==============================================================================
class ChoiceParameterComponent final : public juce::Component,      // drop-down list
    private ParameterListener
{
public:
    ChoiceParameterComponent(juce::AudioProcessor& proc, juce::AudioProcessorParameter& param)
        : ParameterListener(proc, param),
        parameterValues(getParameter().getAllValueStrings())
    {
        link = NULL;

        box.addItemList(parameterValues, 1);
        // Set the initial value.
        handleNewParameterValue();

        box.onChange = [this] { boxChanged(); };
        addAndMakeVisible(box);
    }

    void paint(juce::Graphics&) override {}

    void resized() override
    {
        auto area = getLocalBounds();
        area.removeFromLeft(8);
        box.setBounds(area.reduced(0, 0)); // (0,10)
    }

    void setLink(juce::Component& l)
    {
        link = &l;
    }

    void linkAction()
    {
    }

private:
    void handleNewParameterValue() override
    {
        auto index = parameterValues.indexOf(getParameter().getCurrentValueAsText());

        if (index < 0)
        {
            // The parameter is producing some unexpected text, so we'll do
            // some linear interpolation.
            index = juce::roundToInt(getParameter().getValue() * (float)(parameterValues.size() - 1));
        }

        box.setSelectedItemIndex(index);
    }

    void boxChanged()
    {
        if (getParameter().getCurrentValueAsText() != box.getText())
        {
            getParameter().beginChangeGesture();

            // When a parameter provides a list of strings we must set its
            // value using those strings, rather than a float, because VSTs can
            // have uneven spacing between the different allowed values.
            getParameter().setValueNotifyingHost(getParameter().getValueForText(box.getText()));

            getParameter().endChangeGesture();
        }
    }

    juce::Component* link;
    juce::ComboBox box;
    const juce::StringArray parameterValues;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChoiceParameterComponent)
};

//==============================================================================



//==============================================================================

class ParameterDisplayComponent : public juce::Component
{
public:
    ParameterDisplayComponent(juce::AudioProcessor& processor, juce::AudioProcessorParameter& param, int wdth)
        : parameter(param), paramWidth(wdth)
    {
        link = NULL;

        const juce::Array<juce::AudioProcessorParameter*>& p = processor.getParameters();

        //parameterLabel.setText(parameter.getLabel(), juce::dontSendNotification);
        //addAndMakeVisible(parameterLabel);

        parameterComp = createParameterComp(processor);
        addChildAndSetID(parameterComp.get(), "ActualComponent");
        actualComp = parameterComp.get();

        //setSize(400, 40);
        setSize(paramWidth, 40);

    }

    ~ParameterDisplayComponent() override
    {}

    void paint(juce::Graphics&) override {}

    void displayParameterName(juce::Justification just)
    {
        // substring removes first char indicator (for switch component, circular/horizontal slider etc)
        if (just == juce::Justification::centredBottom || just == juce::Justification::bottom || just == juce::Justification::bottomLeft || just == juce::Justification::bottomRight)
            justName = 'b';
        if (just == juce::Justification::centredRight || just == juce::Justification::right)
            justName = 'r';
        if (just == juce::Justification::centredTop || just == juce::Justification::top)
            justName = 't';
        if (just == juce::Justification::centredLeft || just == juce::Justification::left)
            justName = 'l';

        parameterName.setText(parameter.getName(128).substring(1), juce::dontSendNotification);
        parameterName.setJustificationType(just);
        addAndMakeVisible(parameterName);
    }

    void displayParameterLabel(juce::Justification just)
    {
        // substring removes first char indicator (for switch component, circular/horizontal slider etc)
        if (just == juce::Justification::centredBottom || just == juce::Justification::bottom)
            justLabel = 'B';
        if (just == juce::Justification::centredRight || just == juce::Justification::right)
            justLabel = 'R';
        if (just == juce::Justification::centredTop || just == juce::Justification::top)
            justLabel = 'T';
        if (just == juce::Justification::centredLeft || just == juce::Justification::left)
            justLabel = 'L';

        //Font myFont("Cooper Std", "Black Italic", 10.0f);
        //parameterLabel.setFont(myFont);
        parameterLabel.setText(parameter.getLabel(), juce::dontSendNotification);
        parameterLabel.setJustificationType(just);
        addAndMakeVisible(parameterLabel);
    }

    void resized() override
    {
        auto area = getLocalBounds();

        switch (justName)
        {
        case 'l': parameterName.setBounds(area.removeFromLeft(getWidth() / 4));  break;
        case 't': parameterName.setBounds(area.removeFromTop(getHeight() / 8));  break;
        case 'r': parameterName.setBounds(area.removeFromRight(getWidth() / 4));  break;
        case 'b': parameterName.setBounds(area.removeFromBottom(getHeight() / 8));  break;
            // default : parameterName.setBounds(area.removeFromLeft(getWidth() / 4));
        }


        //parameterLabel.setBounds(area.removeFromRight(50));
        if (paramWidth == 400) // basically... if parentpanel is horizontal
            parameterLabel.setBounds(area.removeFromRight(getWidth() / 8));
        parameterComp->setBounds(area);
    }

    void setLink(juce::Component& l)
    {
        link = &l;
    }

    void linkAction()
    {
    }

    template<typename A>
    A* getParameterComp()
    {
        if (dynamic_cast<A*>(parameterComp.get()) != nullptr)
        {
            return dynamic_cast<A*>(parameterComp.get());
        }
    }




private:
    juce::AudioProcessorParameter& parameter;
    juce::Label parameterName, parameterLabel;
    juce::Component* actualComp;
    juce::Component* link;
    char justName = 'l';
    char justLabel = 'r';
    int paramWidth;
    std::unique_ptr<Component> parameterComp;

    std::unique_ptr<Component> createParameterComp(juce::AudioProcessor& processor) const
    {


        if (parameter.isBoolean())
            if (parameter.getName(128).startsWithChar('b'))
                //indicates an on/off button switch, substring removes the 'B' button indicator in the parameterName
                return std::make_unique<BooleanButtonParameterComponent>(processor, parameter, parameter.getName(128).substring(1));
            else
                return std::make_unique<BooleanParameterComponent>(processor, parameter, parameter.getName(128));

        // Most hosts display any parameter with just two steps as a switch.
        if (parameter.getNumSteps() == 2)
            if (parameter.getName(128).startsWithChar('b'))
                return std::make_unique<SwitchButtonParameterComponent>(processor, parameter);
            else
                return std::make_unique<SwitchParameterComponent>(processor, parameter);


        if (!parameter.getAllValueStrings().isEmpty())
            //&& std::abs(parameter.getNumSteps() - parameter.getAllValueStrings().size()) <= 1)
            if (parameter.getName(128).startsWithChar('b'))
                return std::make_unique<SwitchButtonParameterComponent>(processor, parameter);
            else
                return std::make_unique<ChoiceParameterComponent>(processor, parameter);


        if (parameter.getName(128).startsWithChar('i'))
            return std::make_unique<IncrementParameterComponent>(processor, parameter);

        // Everything else can be represented as a slider.
        return std::make_unique<SliderParameterComponent>(processor, parameter);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterDisplayComponent)
};

//==============================================================================
class ParametersPanel : public juce::Component
{
public:
    ParametersPanel(juce::AudioProcessor& processor, const juce::Array<juce::AudioProcessorParameter*> parameters, bool hrzntl)
        : horizontal(hrzntl)
    {
        if (horizontal)
            paramWidth = 400 / parameters.size();

  
        paramHeight = 40;
        outline = false;

        for (auto* param : parameters)
            if (param->isAutomatable())
                addChildAndSetID(paramComponents.add(new ParameterDisplayComponent(processor, *param, paramWidth)), param->getName(128) + "Comp");

        for (auto* param : parameters)    
            if (param->isAutomatable())
                allComponents.add(new ParameterDisplayComponent(processor, *param, paramWidth));

        maxWidth = 400;
        height = 0;
        if (!horizontal)
        {
            for (auto& comp : paramComponents)
            {
                maxWidth = juce::jmax(maxWidth, comp->getWidth());
                height += comp->getHeight();
            }
        }
        else
        {
            height += 40;
        }
        setSize(maxWidth, juce::jmax(height, 40));

    }

    ~ParametersPanel() override
    {
        allComponents.clear();
        paramComponents.clear();
    }

    void addParameterDisplayComponent(ParameterDisplayComponent* comp, juce::String ID)
    {
        addChildAndSetID(paramComponents.add(comp), ID);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

        if (outline)
        {
            g.setColour(juce::Colours::orange);
            auto area = getLocalBounds();
            g.drawRect(area.reduced(10, 5));
        }

    }

    bool isHorizontal()
    {
        return horizontal;
    }

    void setOutline(bool o)
    {
        outline = o;
    }

    void resized() override
    {
        auto area = getLocalBounds();

        if (horizontal)
        {
            auto row = area.removeFromTop(getHeight());
            for (auto* comp : paramComponents)   // change to allComponents if you start stacking panels horizontally 
                comp->setBounds(row.removeFromLeft(paramWidth));
        }
        else
        {
            for (auto* comp : allComponents)
                comp->setBounds(area.removeFromTop(comp->getHeight()));
        }

    }

    void addPanel(ParametersPanel* p)
    {
        allComponents.add(p);
        addAndMakeVisible(p);
        setSize(maxWidth, getHeight() + p->getHeight());
        auto area = getLocalBounds();
        p->setBounds(area.removeFromBottom(p->getHeight()));
    }

    void addPanel(std::unique_ptr<ParametersPanel> p, juce::String id)  //overload
    {
        auto& o = *p.get();
        allComponents.add(p.get());
        addChildAndSetID(p.get(), id);
        //addAndMakeVisible(o);
        setSize(maxWidth, getHeight() + p->getHeight());
        auto area = getLocalBounds();
        p->setBounds(area.removeFromBottom(p->getHeight()));
    }


public:
    int height;
    int maxWidth;
    int paramWidth = 400;
    int paramHeight = 40;
    juce::OwnedArray<ParameterDisplayComponent> paramComponents;
    juce::OwnedArray<Component> allComponents;

private:
    bool horizontal, outline;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParametersPanel)
};
//===============================================================================================================
class VisualOrbit : public juce::Component,
    private juce::AudioProcessorParameter::Listener
{ 
public:
    VisualOrbit(NewProjectAudioProcessor& p,juce::AudioProcessorValueTreeState& t, juce::AudioProcessorParameter* stepParam, juce::AudioProcessorParameter* pulseParam,int i, juce::Colour c)
        : processor(p),tree(t), stepParameter(*stepParam), pulseParameter(*pulseParam), index(i), color(c)
    {      
        stepParameter.addListener(this);
        pulseParameter.addListener(this);
        
        numSteps = stepParameter.getValue();

        auto myVariable = tree.getRawParameterValue("StepCount"+std::to_string(index+1));
        numSteps = *myVariable;
      
        pulseActive = pulseParameter.getDefaultValue();

        pulseIndex = pulseParameter.getParameterIndex();
        stepIndex = stepParameter.getParameterIndex();



    }

    ~VisualOrbit() override
    {
        stepParameter.removeListener(this);
        pulseParameter.removeListener(this);
    }

    void paint(juce::Graphics& g) override
    {
        setStep();

        auto dotPos = 0.0f;
        auto outline = findColour(Slider::rotarySliderOutlineColourId);
        auto fill = findColour(juce::Slider::rotarySliderFillColourId);

        auto bounds = getLocalBounds();
        auto radius = jmin(bounds.getWidth()-8, bounds.getHeight()-8) / 2.0f;
        //auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        auto toAngle = 0.0f + dotPos * (2*MathConstants<float>::pi - 0.0f);
        auto lineW = jmin(8.0f, radius * 0.5f);
        auto arcRadius = radius - lineW * 0.5f;

        Path backgroundArc;
        backgroundArc.addCentredArc(bounds.getCentreX(),
            bounds.getCentreY(),
            arcRadius,
            arcRadius,
            0.0f,
            0.0f,
            2*MathConstants<float>::pi,
            true);

        g.setColour(outline);
        g.strokePath(backgroundArc, PathStrokeType(lineW, PathStrokeType::curved, PathStrokeType::rounded));

        g.setColour(color.withAlpha(0.5f));
        for (int i = 0; i < numSteps; i++)
        {
            auto lnAngle = (2 * MathConstants<float>::pi / numSteps) * i;
            Point<float> thumbPoint(bounds.getCentreX() + arcRadius * std::cos(lnAngle - MathConstants<float>::halfPi),
                bounds.getCentreY() + arcRadius * std::sin(lnAngle - MathConstants<float>::halfPi));
            auto dotW = lineW * 0.8f;
            g.fillEllipse(Rectangle<float>(dotW, dotW).withCentre(thumbPoint));
        }

        auto thumbWidth = lineW * 1.5f;
        toAngle = (2 * MathConstants<float>::pi / numSteps) * currentStep;
        Point<float> thumbPoint(bounds.getCentreX() + arcRadius * std::cos(toAngle - MathConstants<float>::halfPi),
            bounds.getCentreY() + arcRadius * std::sin(toAngle - MathConstants<float>::halfPi));

        
        g.setColour( (pulseParameter.getValue())? 
            color : juce::Colours::darkgrey);
        g.fillEllipse(Rectangle<float>(thumbWidth, thumbWidth).withCentre(thumbPoint));

    }


    void resized() override
    {
        auto area = getLocalBounds();
    }

   
    void parameterValueChanged(int i, float f ) override 
    {
        //MessageManagerLock mml(Thread::getCurrentThread());
        if (i == stepIndex)
        {
            processor.cycleChanged = true;
        } 
    }

    void setStep()
    {   // thought I might need a MessageManagerLock, looks like we're good here... 
        numSteps = *tree.getRawParameterValue("StepCount"+std::to_string(index+1)); 
    }

    void move(int i) 
    {
        currentStep = i;
    }

    void parameterGestureChanged(int i, bool b) override 
    {
        
    }

public:
    int height;
    int width;
    int index;
    bool pulseActive;
    int numSteps, currentStep;
    int stepIndex, pulseIndex;
    juce::Colour color;

private:
    NewProjectAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& tree;
    juce::AudioProcessorParameter& stepParameter;
    juce::AudioProcessorParameter& pulseParameter;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VisualOrbit)
};
//==============================================================================>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
class OrbitPanel :public juce::Component,
    private juce::AudioProcessorParameter::Listener
{
public:
    OrbitPanel(NewProjectAudioProcessor& proc, juce::AudioProcessorParameter* forceStepParam)
        :processor(proc), step(*forceStepParam)
    {
        setSize(200, 200);
        step.addListener(this);

        addOrbit( std::make_unique<VisualOrbit>(processor, processor.treeState, processor.treeState.getParameter("StepCount1"), processor.treeState.getParameter("PulseActive1"), 0, juce::Colours::white));
        addOrbit( std::make_unique<VisualOrbit>(processor, processor.treeState, processor.treeState.getParameter("StepCount2"), processor.treeState.getParameter("PulseActive2"), 1, juce::Colours::limegreen));
        addOrbit( std::make_unique<VisualOrbit>(processor, processor.treeState, processor.treeState.getParameter("StepCount3"), processor.treeState.getParameter("PulseActive3"), 2, juce::Colours::orange));
        addOrbit( std::make_unique<VisualOrbit>(processor, processor.treeState, processor.treeState.getParameter("StepCount4"), processor.treeState.getParameter("PulseActive4"), 3, juce::Colours::magenta));
        addOrbit( std::make_unique<VisualOrbit>(processor, processor.treeState, processor.treeState.getParameter("StepCount5"), processor.treeState.getParameter("PulseActive5"), 4, juce::Colours::cyan));
      
        
    }
    ~OrbitPanel()
    {
        orbits.clear();
        removeAllChildren();
       
    }
    void paint(juce::Graphics& g) override
    {
        Rectangle<float> area(getX(), getY(), getWidth(), getHeight());
        area = area.reduced(10);
        g.setColour(juce::Colours::grey);
        g.fillRoundedRectangle(area, 15.0f);
    }
    void resized() override
    {
        auto area = getLocalBounds().reduced(10);
       
        if (orbits.size())
        {
            int spacing;
            switch (orbits.size())
            {
                case 2: spacing = 50; break;
                case 3: spacing = 25; break;
                case 4: spacing = 18; break;
                case 5: spacing = 15; break;
                default: spacing = 0; break;
            }

            for (int x = 0; x < orbits.size(); x++)
            {   orbits[x]->setBounds(area.reduced( 55 - (spacing*x)) );    }
         
          
        }
    }

    void addOrbit(std::unique_ptr<VisualOrbit> vo)
    {
        //addChildAndSetID(o,"orbit");
        VisualOrbit& o = *vo;

        indexes.add(o.stepIndex);
        indexes.add(o.stepIndex+1);

        addAndMakeVisible(o);

        orbits.push_back(std::move(vo));
    } 

    void insertOrbit(std::unique_ptr<VisualOrbit> vo)
    {
        VisualOrbit& o = *vo;
        //orbits.insert(orbits.begin() + o.index, vo);
    }

    void removeOrbit(int i)
    {
      
        orbits.erase(orbits.begin()+i);
    }

    void sendCycleChanged()  // should only be pulseCount or stepCount
    {
        processor.cycleChanged = true;  
    }
    
    void parameterValueChanged(int i, float f) override
    {
       
        MessageManagerLock mml(Thread::getCurrentThread());
        {  
            for (int x = 0; x < orbits.size(); x++)
            {
                orbits[x]->move(processor.currentStep[x]);
                repaint();
            }
        }
    }

    void parameterGestureChanged(int i, bool b)
    {
    }
public:
    juce::SortedSet<int> indexes; 
    std::vector<std::unique_ptr<VisualOrbit>> orbits;
    juce::AudioProcessorParameter& step;
    NewProjectAudioProcessor& processor;
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbitPanel)

};
//==================================================================================================================
struct AarrowAudioProcessorEditor::Pimpl
{
    Pimpl(AarrowAudioProcessorEditor& parent) : owner(parent)
    {
        /*auto* p = parent.getAudioProcessor();
         jassert(p != nullptr);
        legacyParameters.update(*p, false);*/

        owner.setOpaque(true);

        params.add(owner.audioProcessor.treeState.getParameter("Speed"));



        myPanel = std::make_unique<ParametersPanel>(owner.audioProcessor, params, false);
        dynamic_cast<SliderParameterComponent*> (myPanel->findChildWithID("SPEEDComp")->findChildWithID("ActualComponent")) ->changeSliderStyle(3);
        params.clear();


        //for (auto* comp : myPanel->getChildren())
        //    auto pie = comp->getComponentID();
        //    //attach breakpoint if you need help checking componentID's
        
       /* params.add(std::make_unique<juce::AudioParameterBool>(juce::String("OnButton" + std::to_string(i)), ju
        params.add(std::make_unique<juce::AudioParameterBool>(juce::String("Reversed" + std::to_string(i)), ju

        params.add(std::make_unique<juce::AudioParameterBool>(juce::String("PulseActive" + std::to_string(i)),


        params.add(std::make_unique<juce::AudioParameterInt>(juce::String("StepCount" + std::to_string(i)), ju
        params.add(std::make_unique<juce::AudioParameterInt>(juce::String("PulseCount" + std::to_string(i)), ju

        params.add(std::make_unique<juce::AudioParameterChoice>(juce::String("OutputNote" + std::to_string(i))

        params.add(std::make_unique<juce::AudioParameterInt>(juce::String("Octave" + std::to_string(i)), juce:*/

        
        
        params.add(owner.audioProcessor.treeState.getParameter("StepCount1"));
        //params.add(owner.audioProcessor.treeState.getParameter("PulseCount1"));
        params.add(owner.audioProcessor.treeState.getParameter("OutputNote1"));
        params.add(owner.audioProcessor.treeState.getParameter("iOctave1"));

        controllerPanel = std::make_unique<ParametersPanel>(owner.audioProcessor, params, true);
        auto stepSlider = dynamic_cast<SliderParameterComponent*> (controllerPanel->findChildWithID("STEPS1Comp")->findChildWithID("ActualComponent"));
        stepSlider->changeSliderStyle(0);
        //params.add(owner.audioProcessor.treeState.getParameter("bOnButton1"));
        //params.add(owner.audioProcessor.treeState.getParameter("Reversed1"));

        clock = std::make_unique<OrbitPanel>(owner.audioProcessor, owner.audioProcessor.treeState.getParameter("ForceStep"));

       
 
        //myPanel->setSize(400, 100);
        //dynamic_cast<ParameterDisplayComponent*> (myPanel->findChildWithID("-RANGEComp"))->displayParameterName(juce::Justification::centredRight);
        //auto RangeSlider = dynamic_cast<SliderParameterComponent*>(myPanel->findChildWithID("-RANGEComp")->findChildWithID("ActualComponent"));
        //RangeSlider->setSize(RangeSlider->getWidth() + 50, RangeSlider->getHeight());
        //RangeSlider->setSliderSkew(0.65f);
        //RangeSlider->changeSliderStyle(3);

        // -------------------------------------------------------------------------------
    
        //controllerPanel->addPanel(new ParametersPanel(owner.audioProcessor, params, true),"TopController1");
        

        // ---------------------------------------------------------------------------------------------
        fullPanel = new Component();
        fullPanel->setSize(600, 250);
        fullPanel->addAndMakeVisible(*myPanel);
        fullPanel->addAndMakeVisible(*clock);
        fullPanel->addAndMakeVisible(*controllerPanel);

        clock->setBounds(fullPanel->getLocalBounds()
            .removeFromRight(210)
            .translated(0,-20));

       controllerPanel->setBounds(fullPanel->getLocalBounds()
            .removeFromLeft(300).removeFromBottom(100));

        myPanel->setBounds(fullPanel->getLocalBounds()
            .removeFromRight(210)
            .removeFromBottom(30)
            .translated(-50,-10));

     


            //ParameterDisplayComponent* SyncComp = dynamic_cast<ParameterDisplayComponent*>(Panel2->findChildWithID("bBPM LinkComp"));
            //SyncComp->getParameterComp<BooleanButtonParameterComponent>()->setLink(*SpeedComp->findChildWithID("ActualComponent"));

        params.clear();
        view.setViewedComponent(fullPanel);

        owner.addAndMakeVisible(view);
        //owner.addAndMakeVisible(tooltipWindow);

        view.setScrollBarsShown(true, false);
    }

    ~Pimpl()
    {
        view.setViewedComponent(nullptr, false);
    }

    void resize(juce::Rectangle<int> size)
    {
        view.setBounds(size);
        auto content = view.getViewedComponent();
        content->setSize(view.getMaximumVisibleWidth(), content->getHeight());
    }

    //==============================================================================
    AarrowAudioProcessorEditor& owner;
    Component* fullPanel;
    juce::Array<juce::AudioProcessorParameter*> params;
    
    
private:
    juce::TooltipWindow tooltipWindow;
    std::vector<std::unique_ptr<ParametersPanel>> orbitControllers;
    std::unique_ptr<OrbitPanel> clock;
    std::unique_ptr<ParametersPanel> myPanel;
    std::unique_ptr<ParametersPanel> controllerPanel;
public:
    juce::Viewport view;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Pimpl)
};
//==============================================================================
AarrowAudioProcessorEditor::AarrowAudioProcessorEditor(NewProjectAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), pimpl(new Pimpl(*this))
{

    setLookAndFeel(&Aalf);
    setSize(pimpl->view.getViewedComponent()->getWidth() + pimpl->view.getVerticalScrollBar().getWidth(),
        juce::jmin(pimpl->view.getViewedComponent()->getHeight(), 400));



}

AarrowAudioProcessorEditor::~AarrowAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

//==============================================================================
void AarrowAudioProcessorEditor::paint(juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    /* g.setColour (juce::Colours::white);
     g.setFont (15.0f);
     g.drawFittedText ("Please Work", getLocalBounds(), juce::Justification::centred, 1);*/
}

void AarrowAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    pimpl->resize(getLocalBounds());
}

//===================================================================================


