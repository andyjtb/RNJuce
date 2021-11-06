# RNJuce

This is an example project combining JUCE with React Native

The JUCE side just plays a constant 440 Hz sine wave

The React Native side is the blank app

To run:

* Checkout this repo

* `cd` to the repo

* Init submodules: `git submodule update --init --recursive`

* Install JS: `yarn install`

* Run `init-project.py` to build the JUCE Project, add the React Native Cocoa Pods and patch the required changes

* Open the .xcworkspace file and build