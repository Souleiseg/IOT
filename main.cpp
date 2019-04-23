#include "mbed.h"
#include "zest-radio-atzbrf233.h"
#include "MQTTNetwork.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"

// Network interface
NetworkInterface *net;

namespace {
#define PERIOD_MS 5000
}

static DigitalOut led1(LED1);
I2C i2c(I2C1_SDA, I2C1_SCL);
uint8_t lm75_adress = 0x48 << 1;

AnalogIn moisture(ADC_IN1);

float humidity_max = 0.8;

int arrivedcount = 0;

//On ajoute les topics de température et d'humidité et on/off
const char* topic1 = "LeCoqGalmotSeghir/feeds/projectiotfeeds.tempresult";
const char* topic2 = "LeCoqGalmotSeghir/feeds/projectiotfeeds.humiresult";
const char* topic3 = "LeCoqGalmotSeghir/feeds/projectiotfeeds.onofftrigger";

/* Printf the message received and its configuration */
void messageArrived(MQTT::MessageData& md)
{
    MQTT::Message &message = md.message;
    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", message.qos, message.retained, message.dup, message.id);
    printf("Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
    ++arrivedcount;

	char temp[20];
    strncpy(temp, (char *) message.payload, message.payloadlen);

    //allumer ou éteindre la led en fonction de l'état du bouton
    if (strcmp((const char*) temp, "ON") == 0) {
    	printf("LED ON\n");
    	led1 = 1;
    }else {
    	(printf("LED OFF\n"));
    	led1 = 0;
    }

}

// MQTT
int main() {

	float humidity = 0.0f;

	char cmd[2];
	cmd[0] = 0x00; // adresse registre temperature
	i2c.write(lm75_adress, cmd, 1);
	i2c.read(lm75_adress, cmd, 2);
	float temperature = ((cmd[0] << 8 | cmd[1] ) >> 7) * 0.5;


	int result;

    // Add the border router DNS to the DNS table
    nsapi_addr_t new_dns = {
        NSAPI_IPv6,
        { 0xfd, 0x9f, 0x59, 0x0a, 0xb1, 0x58, 0, 0, 0, 0, 0, 0, 0, 0, 0x00, 0x01 }
    };
    nsapi_dns_add_server(new_dns);

    printf("Starting MQTT demo\n");

    // Get default Network interface (6LowPAN)
    net = NetworkInterface::get_default_instance();
    if (!net) {
        printf("Error! No network interface found.\n");
        return 0;
    }

    // Connect 6LowPAN interface
    result = net->connect();
    if (result != 0) {
        printf("Error! net->connect() returned: %d\n", result);
        return result;
    }

    // Build the socket that will be used for MQTT
    MQTTNetwork mqttNetwork(net);

    // Declare a MQTT Client
    MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);

    // Connect the socket to the MQTT Broker
    const char* hostname = "io.adafruit.com";
    uint16_t port = 1883;
    printf("Connecting to %s:%d\r\n", hostname, port);
    int rc = mqttNetwork.connect(hostname, port);
    if (rc != 0)
        printf("rc from TCP connect is %d\r\n", rc);

    // Connect the MQTT Client
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = "92f03640-f570-4f80-b618-1b0dd4513f41"; //Généré sur internet
    data.username.cstring = "LeCoqGalmotSeghir";
    data.password.cstring = "0b08240c0f604506ac5b71943ba16c73";
    if ((rc = client.connect(data)) != 0)
        printf("rc from MQTT connect is %d\r\n", rc);

    // Subscribe to the same topic we will publish in
    if((rc = client.subscribe(topic3, MQTT::QOS2, messageArrived)) != 0)
            printf("rc from MQTT subscribe is %d\r\n", rc);

    // Send a message with QoS 0
    MQTT::Message message;

    // QoS 0
    char buf[100];

    //Boucle qui récupère les valeurs de la température et de l'humidité toutes les 5 secondes
    while(true) {

    	cmd[0] = 0x00; // adresse registre temperature
    	i2c.write(lm75_adress, cmd, 1);
    	i2c.read(lm75_adress, cmd, 2);
    	temperature = ((cmd[0] << 8 | cmd[1] ) >> 7) * 0.5;

    	humidity = moisture.read() * 100 / humidity_max;

    	//Affichage de la température
    	sprintf(buf, "%.2f", temperature);
    	printf("Temperature : %.2f\n", temperature);

    	message.qos = MQTT::QOS0;
    	message.retained = false;
    	message.dup = false;
    	message.payload = (void*)buf;
    	message.payloadlen = strlen(buf)+1;

    	rc = client.publish(topic1, message);

    	//Affichage de l'humidité
    	sprintf(buf, "%.2f", humidity);
    	printf("Humidity : %.2f\n", humidity);
    	message.qos = MQTT::QOS0;
    	message.retained = false;
    	message.dup = false;
    	message.payload = (void*)buf;
    	message.payloadlen = strlen(buf)+1;

    	rc = client.publish(topic2, message);
    	client.yield(100);
    	ThisThread::sleep_for(PERIOD_MS);
    }

    // yield function is used to refresh the connexion
    // Here we yield until we receive the message we sent
    while (arrivedcount < 1)
        client.yield(100);

    // Disconnect client and socket
    client.disconnect();
    mqttNetwork.disconnect();

    // Bring down the 6LowPAN interface
    net->disconnect();
    printf("Done\n");
}

