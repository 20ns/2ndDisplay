using System;
using System.Net.NetworkInformation;
using System.Net;

class AndroidDiscoveryTest 
{
    static void Main() 
    {
        Console.WriteLine("Direct Android Discovery Test");
        Console.WriteLine("============================");
        
        NetworkInterface[] interfaces = NetworkInterface.GetAllNetworkInterfaces();
        
        foreach (NetworkInterface ni in interfaces) 
        {
            if (ni.OperationalStatus != OperationalStatus.Up)
                continue;
                
            Console.WriteLine($"\nInterface: {ni.Name}");
            Console.WriteLine($"Description: {ni.Description}");
            Console.WriteLine($"Type: {ni.NetworkInterfaceType}");
            
            // Check if this looks like Android
            string desc = ni.Description.ToLower();
            bool isAndroid = desc.Contains("remote ndis") || 
                           desc.Contains("rndis") || 
                           desc.Contains("android") ||
                           (desc.Contains("ethernet") && ni.Name.Contains("3"));
            
            if (isAndroid) 
            {
                Console.WriteLine(">>> POTENTIAL ANDROID INTERFACE <<<");
                
                // Get IP addresses
                IPInterfaceProperties ipProps = ni.GetIPProperties();
                foreach (UnicastIPAddressInformation ip in ipProps.UnicastAddresses) 
                {
                    if (ip.Address.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork) 
                    {
                        Console.WriteLine($"IP: {ip.Address}");
                        
                        // Calculate likely gateway (x.x.x.1)
                        byte[] bytes = ip.Address.GetAddressBytes();
                        bytes[3] = 1;
                        IPAddress gateway = new IPAddress(bytes);
                        Console.WriteLine($"Likely Android Gateway: {gateway}");
                        
                        // Test connectivity
                        Console.WriteLine($"Testing UDP to {gateway}:5004...");
                        try 
                        {
                            using (var client = new System.Net.Sockets.UdpClient())
                            {
                                client.Connect(gateway, 5004);
                                byte[] data = System.Text.Encoding.UTF8.GetBytes("PING");
                                client.Send(data, data.Length);
                                Console.WriteLine("✅ UDP packet sent successfully!");
                            }
                        }
                        catch (Exception ex)
                        {
                            Console.WriteLine($"❌ UDP send failed: {ex.Message}");
                        }
                    }
                }
            }
        }
    }
}
