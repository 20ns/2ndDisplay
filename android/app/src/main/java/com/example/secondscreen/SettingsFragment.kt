package com.example.secondscreen

import android.content.Intent
import android.os.Bundle
import androidx.preference.ListPreference
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat

class SettingsFragment : PreferenceFragmentCompat() {

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.preferences, rootKey)

        // Configure display mode preference
        val displayModePref = findPreference<ListPreference>("display_mode")
        displayModePref?.setOnPreferenceChangeListener { _, newValue ->
            val mode = newValue.toString()

            // Update the display mode in the service
            val intent = Intent(requireContext(), VideoReceiverService::class.java)
            intent.action = VideoReceiverService.ACTION_CHANGE_MODE
            intent.putExtra("mode", mode)

            requireContext().startService(intent)
            true
        }
    }
}
