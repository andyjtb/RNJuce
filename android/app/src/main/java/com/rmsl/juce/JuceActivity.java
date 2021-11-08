package com.rmsl.juce;

import com.facebook.react.ReactActivity;
import android.content.Intent;

public class JuceActivity extends ReactActivity {

  /**
   * Returns the name of the main component registered from JavaScript. This is used to schedule
   * rendering of the component.
   */
  @Override
  protected String getMainComponentName() {
    return "RNJuce";
  }

       //==============================================================================
     private native void appNewIntent (Intent intent);

     @Override
     public void onNewIntent (Intent intent)
     {
         super.onNewIntent(intent);
         setIntent(intent);

         appNewIntent (intent);
     }
}
