#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    setOpaque (true);
    addAndMakeVisible (liveAudioScroller);

    addAndMakeVisible (explanationLabel);
    explanationLabel.setFont (juce::Font (15.0f, juce::Font::plain));
    explanationLabel.setJustificationType (juce::Justification::topLeft);
    explanationLabel.setEditable (false, false, false);
    explanationLabel.setColour (juce::TextEditor::textColourId, juce::Colours::black);
    explanationLabel.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0x00000000));

    addAndMakeVisible (recordButton);
    recordButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffff5c5c));
    recordButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);

    recordButton.onClick = [this]
    {
        if (recorder.isRecording())
            stopRecording();
        else
            startRecording();
    };

    addAndMakeVisible (recordingThumbnail);

    setSize (800, 600);

    if (juce::RuntimePermissions::isRequired (juce::RuntimePermissions::recordAudio)
        && ! juce::RuntimePermissions::isGranted (juce::RuntimePermissions::recordAudio))
    {
        juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio,
                                           [&] (bool granted) { setAudioChannels (granted ? 2 : 0, 2); });
    }
    else
    {
        setAudioChannels (2, 2);
    }

#if ! (JUCE_ANDROID || JUCE_IOS)
    addAndMakeVisible (settingsButton);
    settingsButton.onClick = [this]
    {
        auto* comp = new juce::AudioDeviceSelectorComponent (deviceManager, 0, 2, 0, 2, false, false, true, true);

        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned (comp);

        const auto width = 500;
        const auto height = 200;

        juce::Rectangle<int> area (0, 0, width, height);

        options.content->setSize (area.getWidth(), area.getHeight());
        options.content->setWantsKeyboardFocus (false);

        options.dialogTitle                   = "Audio Settings";
        options.dialogBackgroundColour        = juce::Colour (0xff0e345a);
        options.escapeKeyTriggersCloseButton  = true;
        options.useNativeTitleBar             = true;
        options.resizable                     = true;

        settingsPage = options.create();
        settingsPage->addToDesktop();
        settingsPage->setVisible (true);

        if (settingsPage != nullptr)
        {
            if (const auto* parentWindow = findParentComponentOfClass<juce::DocumentWindow>())
                settingsPage->setBounds (parentWindow->getRight(), parentWindow->getY(), width, height);
            else
                settingsPage->centreWithSize (width, height);
        }
    };
#endif

    level = 1.f;
}

MainComponent::~MainComponent()
{
    if (auto* c = settingsPage.getComponent())
        settingsPage.deleteAndZero();

    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    liveAudioScroller.audioDeviceAboutToStart (nullptr);
    recorder.audioDeviceAboutToStart (deviceManager.getCurrentAudioDevice());

    currentSampleRate = sampleRate;

    frequency = 440;
    level = 0.5f;
    auto cyclesPerSample = frequency / currentSampleRate;
    angleDelta = cyclesPerSample * 2.0 * juce::MathConstants<double>::pi;
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
#if (JUCE_ANDROID || JUCE_IOS)
    bufferToFill.clearActiveBufferRegion();

    auto* leftBuffer  = bufferToFill.buffer->getWritePointer (0, bufferToFill.startSample);
    auto* rightBuffer = bufferToFill.buffer->getWritePointer (1, bufferToFill.startSample);

    for (auto sample = 0; sample < bufferToFill.numSamples; ++sample)
    {
        auto currentSample = (float) std::sin (currentAngle);
        currentAngle += angleDelta;
        leftBuffer[sample]  = currentSample * level;
        rightBuffer[sample] = currentSample * level;
    }

#else
    liveAudioScroller.audioDeviceIOCallback (bufferToFill.buffer->getArrayOfReadPointers(), 2,
                                             bufferToFill.buffer->getArrayOfWritePointers(), 2,
                                             bufferToFill.numSamples);

    recorder.audioDeviceIOCallback (bufferToFill.buffer->getArrayOfReadPointers(), 2,
                                    bufferToFill.buffer->getArrayOfWritePointers(), 2,
                                    bufferToFill.numSamples);

    bufferToFill.clearActiveBufferRegion();
#endif

}

void MainComponent::releaseResources()
{
    liveAudioScroller.audioDeviceStopped();
    recorder.audioDeviceStopped();
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto area = getLocalBounds();

#if (JUCE_ANDROID || JUCE_IOS)
    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        area.removeFromTop (display->safeAreaInsets.getTop());
#endif

    liveAudioScroller .setBounds (area.removeFromTop (80).reduced (8));
    recordingThumbnail.setBounds (area.removeFromTop (80).reduced (8));
#if (JUCE_ANDROID || JUCE_IOS)
    recordButton      .setBounds (area.removeFromTop (66).removeFromLeft (140).reduced (8));
#endif
    explanationLabel  .setBounds (area.reduced (8));

    settingsButton.setBounds (recordButton.getBounds().translated (recordButton.getWidth() + 20, 0));
}

void MainComponent::startRecording()
{
    if (! juce::RuntimePermissions::isGranted (juce::RuntimePermissions::writeExternalStorage))
    {
        SafePointer<MainComponent> safeThis (this);

        juce::RuntimePermissions::request (juce::RuntimePermissions::writeExternalStorage,
                                     [safeThis] (bool granted) mutable
                                     {
                                         if (granted)
                                             safeThis->startRecording();
                                     });
        return;
    }

   #if (JUCE_ANDROID || JUCE_IOS)
    auto parentDir = juce::File::getSpecialLocation (juce::File::tempDirectory);
   #else
    auto parentDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
   #endif

    lastRecording = parentDir.getNonexistentChildFile ("JUCE Demo Audio Recording", ".wav");

    recorder.startRecording (lastRecording);

    recordButton.setButtonText ("Stop");
    recordingThumbnail.setDisplayFullThumbnail (false);
}

void MainComponent::stopRecording()
{
    recorder.stop();

   #if JUCE_CONTENT_SHARING
    SafePointer<MainComponent> safeThis (this);
    juce::File fileToShare = lastRecording;

    juce::ContentSharer::getInstance()->shareFiles (juce::Array<juce::URL> ({juce::URL (fileToShare)}),
                                              [safeThis, fileToShare] (bool success, const juce::String& error)
                                              {
                                                  if (fileToShare.existsAsFile())
                                                      fileToShare.deleteFile();

                                                  if (! success && error.isNotEmpty())
                                                      juce::NativeMessageBox::showAsync (juce::MessageBoxOptions()
                                                                                     .withIconType (juce::MessageBoxIconType::WarningIcon)
                                                                                     .withTitle ("Sharing Error")
                                                                                     .withMessage (error),
                                                                                   nullptr);
                                              });
   #endif

    lastRecording = juce::File();
    recordButton.setButtonText ("Record");
    recordingThumbnail.setDisplayFullThumbnail (true);
}

