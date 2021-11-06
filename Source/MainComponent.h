#pragma once

#include <JuceHeader.h>

#include "AudioLiveScrollingDisplay.h"

class AudioRecorder  : public juce::AudioIODeviceCallback
{
public:
    AudioRecorder (juce::AudioThumbnail& thumbnailToUpdate)
        : thumbnail (thumbnailToUpdate)
    {
        backgroundThread.startThread();
    }

    ~AudioRecorder() override
    {
        stop();
    }

    //==============================================================================
    void startRecording (const juce::File& file)
    {
        stop();

        if (sampleRate > 0)
        {
            // Create an OutputStream to write to our destination file...
            file.deleteFile();

            if (auto fileStream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
            {
                // Now create a WAV writer object that writes to our output stream...
                juce::WavAudioFormat wavFormat;

                if (auto writer = wavFormat.createWriterFor (fileStream.get(), sampleRate, 1, 16, {}, 0))
                {
                    fileStream.release(); // (passes responsibility for deleting the stream to the writer object that is now using it)

                    // Now we'll create one of these helper objects which will act as a FIFO buffer, and will
                    // write the data to disk on our background thread.
                    threadedWriter.reset (new juce::AudioFormatWriter::ThreadedWriter (writer, backgroundThread, 32768));

                    // Reset our recording thumbnail
                    thumbnail.reset (writer->getNumChannels(), writer->getSampleRate());
                    nextSampleNum = 0;

                    // And now, swap over our active writer pointer so that the audio callback will start using it..
                    const juce::ScopedLock sl (writerLock);
                    activeWriter = threadedWriter.get();
                }
            }
        }
    }

    void stop()
    {
        // First, clear this pointer to stop the audio callback from using our writer object..
        {
            const juce::ScopedLock sl (writerLock);
            activeWriter = nullptr;
        }

        // Now we can delete the writer object. It's done in this order because the deletion could
        // take a little time while remaining data gets flushed to disk, so it's best to avoid blocking
        // the audio callback while this happens.
        threadedWriter.reset();
    }

    bool isRecording() const
    {
        return activeWriter.load() != nullptr;
    }

    //==============================================================================
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override
    {
        sampleRate = device->getCurrentSampleRate();
    }

    void audioDeviceStopped() override
    {
        sampleRate = 0;
    }

    void audioDeviceIOCallback (const float** inputChannelData, int numInputChannels,
                                float** outputChannelData, int numOutputChannels,
                                int numSamples) override
    {
        const juce::ScopedLock sl (writerLock);

        if (activeWriter.load() != nullptr && numInputChannels >= thumbnail.getNumChannels())
        {
            activeWriter.load()->write (inputChannelData, numSamples);

            // Create an AudioBuffer to wrap our incoming data, note that this does no allocations or copies, it simply references our input data
            juce::AudioBuffer<float> buffer (const_cast<float**> (inputChannelData), thumbnail.getNumChannels(), numSamples);
            thumbnail.addBlock (nextSampleNum, buffer, 0, numSamples);
            nextSampleNum += numSamples;
        }

        // We need to clear the output buffers, in case they're full of junk..
        for (int i = 0; i < numOutputChannels; ++i)
            if (outputChannelData[i] != nullptr)
                juce::FloatVectorOperations::clear (outputChannelData[i], numSamples);
    }

private:
    juce::AudioThumbnail& thumbnail;
    juce::TimeSliceThread backgroundThread { "Audio Recorder Thread" }; // the thread that will write our audio data to disk
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter; // the FIFO used to buffer the incoming data
    double sampleRate = 0.0;
    int64_t nextSampleNum = 0;

    juce::CriticalSection writerLock;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeWriter { nullptr };
};

//==============================================================================
class RecordingThumbnail  : public juce::Component,
                            private juce::ChangeListener
{
public:
    RecordingThumbnail()
    {
        formatManager.registerBasicFormats();
        thumbnail.addChangeListener (this);
    }

    ~RecordingThumbnail() override
    {
        thumbnail.removeChangeListener (this);
    }

    juce::AudioThumbnail& getAudioThumbnail()     { return thumbnail; }

    void setDisplayFullThumbnail (bool displayFull)
    {
        displayFullThumb = displayFull;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::darkgrey);
        g.setColour (juce::Colours::lightgrey);

        if (thumbnail.getTotalLength() > 0.0)
        {
            auto endTime = displayFullThumb ? thumbnail.getTotalLength()
                                            : juce::jmax (30.0, thumbnail.getTotalLength());

            auto thumbArea = getLocalBounds();
            thumbnail.drawChannels (g, thumbArea.reduced (2), 0.0, endTime, 1.0f);
        }
        else
        {
            g.setFont (14.0f);
            g.drawFittedText ("(No file recorded)", getLocalBounds(), juce::Justification::centred, 2);
        }
    }

private:
    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache  { 10 };
    juce::AudioThumbnail thumbnail            { 512, formatManager, thumbnailCache };

    bool displayFullThumb = false;

    void changeListenerCallback (juce::ChangeBroadcaster* source) override
    {
        if (source == &thumbnail)
            repaint();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecordingThumbnail)
};


//==============================================================================
class MainComponent  : public juce::AudioAppComponent
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    //==============================================================================

    void startRecording();
    void stopRecording();

    LiveScrollingAudioDisplay liveAudioScroller;
    RecordingThumbnail recordingThumbnail;
    AudioRecorder recorder  { recordingThumbnail.getAudioThumbnail() };

    juce::Label explanationLabel  { {}, "This page demonstrates how to record a wave file from the live audio input..\n\n"
                                 #if (JUCE_ANDROID || JUCE_IOS)
                                  "After you are done with your recording you can share with other apps."
                                 #else
                                  "Pressing record will start recording a file in your \"Documents\" folder."
                                 #endif
                             };
    juce::TextButton recordButton { "Record" };
    juce::TextButton settingsButton { "Settings" };
    juce::File lastRecording;

    SafePointer<juce::DialogWindow> settingsPage;

    double frequency, level = 0.0, currentSampleRate = 0.0, currentAngle = 0.0, angleDelta = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
