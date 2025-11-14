#!/usr/bin/env python3
"""
Brightness control module for power saving mode
"""
import os
import subprocess

class BrightnessControl:
    def __init__(self):
        self.original_brightness = None
        self.brightness_interface = self._find_brightness_interface()
    
    def _find_brightness_interface(self):
        """Find the brightness control interface"""
        # Common brightness paths
        paths = [
            '/sys/class/backlight/intel_backlight',
            '/sys/class/backlight/acpi_video0',
            '/sys/class/backlight/amdgpu_bl0',
            '/sys/class/backlight/nvidia_0',
            '/sys/class/backlight/radeon_bl0',
        ]
        
        for path in paths:
            if os.path.exists(path):
                return path
        
        # Try to find any backlight
        backlight_dir = '/sys/class/backlight'
        if os.path.exists(backlight_dir):
            devices = os.listdir(backlight_dir)
            if devices:
                return os.path.join(backlight_dir, devices[0])
        
        return None
    
    def get_current_brightness(self):
        """Get current brightness level"""
        if not self.brightness_interface:
            return None
        
        try:
            brightness_file = os.path.join(self.brightness_interface, 'brightness')
            with open(brightness_file, 'r') as f:
                return int(f.read().strip())
        except:
            return None
    
    def get_max_brightness(self):
        """Get maximum brightness level"""
        if not self.brightness_interface:
            return None
        
        try:
            max_file = os.path.join(self.brightness_interface, 'max_brightness')
            with open(max_file, 'r') as f:
                return int(f.read().strip())
        except:
            return None
    
    def set_brightness(self, value):
        """Set brightness to specific value"""
        if not self.brightness_interface:
            return False
        
        try:
            brightness_file = os.path.join(self.brightness_interface, 'brightness')
            # Need root to write to brightness
            subprocess.run(['sh', '-c', f'echo {value} > {brightness_file}'], 
                         check=True, stderr=subprocess.DEVNULL)
            return True
        except:
            return False
    
    def set_brightness_percent(self, percent):
        """Set brightness as percentage (0-100)"""
        max_brightness = self.get_max_brightness()
        if max_brightness is None:
            return False
        
        value = int((percent / 100.0) * max_brightness)
        value = max(1, min(value, max_brightness))  # Ensure between 1 and max
        return self.set_brightness(value)
    
    def save_current_brightness(self):
        """Save current brightness to restore later"""
        self.original_brightness = self.get_current_brightness()
    
    def restore_brightness(self):
        """Restore original brightness"""
        if self.original_brightness is not None:
            return self.set_brightness(self.original_brightness)
        return False
    
    def get_brightness_percent(self):
        """Get current brightness as percentage"""
        current = self.get_current_brightness()
        max_brightness = self.get_max_brightness()
        
        if current is None or max_brightness is None:
            return None
        
        return int((current / max_brightness) * 100)
